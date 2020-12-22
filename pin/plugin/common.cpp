#include "common.h"

// #define TRACK_INSTALLED_SIGNAL_HANDLERS

#include <fstream>
#include <set>

// getcwd
#include <unistd.h>

// PROT_READ, PROT_EXEC
#include <sys/mman.h>
// SYS_*
#include <sys/syscall.h>

// signal handling
#ifdef TRACK_INSTALLED_SIGNAL_HANDLERS
#include <signal.h>

std::set<ADDRINT> signal_handler_addresses;
#endif

// multithread support
PIN_LOCK pinLock;
TLS_KEY tls_key = INVALID_TLS_KEY;

// address of the tracee's entry point
ADDRINT entry_address = INVALID_ADDRESS;
BOOL entry_reached = false;

// address of the tracee's exit point
ADDRINT exit_address = INVALID_ADDRESS;
BOOL exit_reached = false;

// contains the directory in which to generate the output files
std::string workdir;

// can be filled with full paths to libraries that should not be traces
std::vector<std::string> skip_libs;

// constains string representations of disassembled instructions
DISASM_CONTAINER *disassemblies;

// can be set to a specific address (usually set in ImageLoad) to initiate a full instruction trace
static ADDRINT trace_start_address = INVALID_ADDRESS;

// address of the __restore_rt C-library function, set in ImageLoad
static ADDRINT address_of_restore_rt = INVALID_ADDRESS;

/* Contains the loaded images before the first thread is started.
 * Starting from the first thread, this information is stored in the ThreadData data structure. */
static std::vector<std::string> global_loads;

VOID StopMeasurements(THREADID threadid) {
  PIN_GetLock(&pinLock, threadid+1);
  exit_reached = true;
  PIN_ReleaseLock(&pinLock);
}

VOID PauseMeasurements(THREADID threadid, ThreadData *tdata, ADDRINT resume_address) {
  ossi(os);
  os << "pausing measurements until " << InstructionLocation(resume_address) << std::endl;
  PRINT((os.str().c_str()));

  // pause the trace process
  tdata->pause_trace_till = resume_address;
}

VOID Exit() {
  fflush(stdout);
  PIN_ExitProcess(1);
}

VOID Exit(std::ostringstream& os, THREADID threadid) {
  PRINT((os.str().c_str()));
  Exit();
}

/* Pretty-print the location of a given instruction's address in terms of its
 * binary image, section and address within that image. */
std::string InstructionLocation(ADDRINT ins) {
  if ((ins == INVALID_ADDRESS)
      || (ins == INVALID_DECODED_ADDRESS))
    return "???";

  PIN_LockClient();
  IMG image = IMG_FindByAddress(ins);
  RTN rtn = RTN_FindByAddress(ins);
  PIN_UnlockClient();

  if (!RTN_Valid(rtn)) {
    std::ostringstream os;
    os << "?rtn@0x" << std::hex << ins;
    return os.str();
  }

  PIN_LockClient();
  SEC section = RTN_Sec(RTN_FindByAddress(ins));
  ADDRINT offset = ins - IMG_LowAddress(image);
  PIN_UnlockClient();

  std::ostringstream os;
  os << IMG_Name(image)
      << "(" << SEC_Name(section) << ")"
      << "+0x" << std::hex << offset;

  return os.str();
}

VOID Backtrace(std::ostringstream& os, const CONTEXT *cxt) {
  ADDRINT addresses[1024];
  PIN_LockClient();
  int nr_addresses = PIN_Backtrace(cxt, reinterpret_cast<void**>(&addresses), 1024);
  PIN_UnlockClient();

  os << std::endl << "=== Backtrace (" << std::dec << (nr_addresses+1) << " entries)" << std::endl;
  for (int i = 0; i < nr_addresses; i++)
    os << "[" << std::dec << i << "] 0x" << std::hex << addresses[i] << " at " << InstructionLocation(addresses[i]) << std::endl;
  os << "=== (end of backtrace)" << std::endl;
}

VOID Calltrace(std::ostringstream& os, ThreadData *tdata) {
  os << std::endl << "=== Call trace (" << std::dec << (tdata->current_call_depth) << " entries)" << std::endl;
  for (int i = tdata->current_call_depth; i >= 0; i--)
    os << "[" << std::dec << i << "] 0x" << std::hex << (tdata->call_continuation_points[i].continuation_point) << " at " << InstructionLocation(tdata->call_continuation_points[i].continuation_point) << std::endl;
  os << "=== (end of call trace)" << std::endl;
}

/* Encode an address for easy storage in the bookkeeping data structures. */
ADDRINT EncodeJump(ADDRINT address, BOOL taken) {
  if (taken)
    address |= TAKEN_BIT;

  return address;
}

/* Decode an encoded address from the bookkeeping data structures. */
ADDRINT DecodeJump(ADDRINT encoded, BOOL& taken, ADDRINT& type) {
  taken = (encoded & TAKEN_BIT);
  type = (encoded & TYPE_MASK) >> TYPE_BIT;

  return encoded & MAX_ADDRESS;
}

/* Pretty-print a branch location along with its (not-)taken information. */
std::string BranchLocation(ADDRINT address, BOOL taken) {
  std::ostringstream os;
  os << "(" << InstructionLocation(address) << ", " << taken << ")";

  return os.str();
}

/* Properly indent a printed string.
 * As PIN doesn't support C++11 in its own STL implementation, we can't return
 * an ostringstream instance here. Instead, we have to create it within the scope
 * of the parent function and pass it by reference to this function. */
VOID IndentedOSS(std::ostringstream& os, int depth) {
  for (int i = 0; i < depth; i++)
    os << " ";
}

BOOL AddressInGeneratedCode(ThreadData* tdata, ADDRINT address) {
  for (auto p : tdata->code_regions) {
    ADDRINT begin = p.first;
    ADDRINT end = p.second;

    // we're working with a sorted data structure, so we can exit early
    if (address < begin)
      break;

    if ((begin <= address) && (address <= end))
      return true;
  }

  return false;
}

/* Callback function to be called upon executing __libc_start_main.
 * The passed argument contains the address of the program's main routine. */
VOID LibcStartMainCalled(THREADID threadid, ADDRINT address_of_main) {
  oss(os);
  os << "__libc_start_main called with address of program's main routine at"
      << " 0x" << std::hex << address_of_main
      << ": " << InstructionLocation(address_of_main) << std::endl;
  PRINT((os.str().c_str()));

  entry_address = address_of_main;
}

/* Callback function to be called upon (dynamically) loading an shared library. */
VOID ImageLoad(IMG img, VOID *v) {
  THREADID threadid = PIN_ThreadId();
  if (threadid == INVALID_THREADID) {
    oss(os);
    os << "ERROR: INVALID_THREAD_ID" << std::endl;
    Exit(os, 0);
  }

  oss(os);
  os << "ImageLoad " << IMG_Name(img)
      << " @ 0x" << std::hex << IMG_LowAddress(img)
      << (IMG_IsMainExecutable(img) ? " *main program*" : "")
      << std::endl;
  PRINT((os.str().c_str()));

  // log the loaded program sections to an output file
  for (SEC sec = IMG_SecHead(img); SEC_Valid(sec); sec = SEC_Next(sec)) {
    std::ostringstream os;

    // threadid,image-name,is-main-executable,section-name,section-first-byte,section-last-byte
    os << threadid
        << "," << IMG_Name(img)
        << "," << IMG_IsMainExecutable(img)
        << "," << SEC_Name(sec)
        << "," << "0x" << std::hex << SEC_Address(sec) << ",0x" << (SEC_Address(sec) + SEC_Size(sec) - 1)
        << std::endl;

    PRINT((os.str().c_str()));

    // record the loaded string
    ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));

    if (!tdata)
      global_loads.push_back(os.str());
    else
      tdata->loads.push_back(os.str());

    if (IMG_Name(img) == "/lib/x86_64-linux-gnu/libc.so.6"
        && SEC_Name(sec) == ".text") {
      // have to look these up manually using a libc.so.6 version with debug symbols (eu-unstrip on Debian)
      ADDRINT restore_rt = 0x37840;
      ADDRINT text_base = 0x22320;

      address_of_restore_rt = SEC_Address(sec) + (restore_rt - text_base);
    }
  }

  // find __libc_start_main so we can start tracing at the application's entry point proper
  RTN libc_start_main_rtn = RTN_FindByName(img, "__libc_start_main");
  if (RTN_Valid(libc_start_main_rtn)) {
    RTN_Open(libc_start_main_rtn);
    RTN_InsertCall(libc_start_main_rtn, IPOINT_BEFORE, (AFUNPTR)LibcStartMainCalled, IARG_THREAD_ID, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);
    RTN_Close(libc_start_main_rtn);
  }
}

/* Callback function to be called upon unloading a (dynamically) loaded shared library. */
VOID ImageUnload(IMG img, VOID *v) {
  THREADID threadid = PIN_ThreadId();

  if (threadid == INVALID_THREADID) {
    oss(os);
    os << "ERROR: INVALID_THREAD_ID" << std::endl;
    Exit(os, 0);
  }

  PRINT(("[%d] ImageUnload %s\n", threadid, IMG_Name(img).c_str()));
}

BOOL GenericAtCondBranch(THREADID threadid, ADDRINT ip, ADDRINT target, ADDRINT fallthrough, BOOL taken) {
  ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));

  // don't do anything when the program's entry point is not reached yet
  if (!entry_reached || exit_reached)
    return false;

  // don't do anything when tracing is paused
  if (tdata->pause_trace_till != INVALID_ADDRESS)
    return false;

  if (DEBUG_CONDITION || tdata->verbose)
  {
    // verbose output
    ossi(os);
    os << ((fallthrough == INVALID_ADDRESS) ? "un" : "") << "cond-jmp"
        << "[" << tdata->current_call_depth << "]"
        << " " << BranchLocation(ip, taken)
        << " -> " << InstructionLocation(target)
        << std::endl;

    PRINT((os.str().c_str()));
  }

  return true;
}

BOOL GenericAtBranch(THREADID threadid, ADDRINT ip, ADDRINT target, BOOL taken) {
  ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));

  // don't do anything when the program's entry point is not reached yet
  if (!entry_reached || exit_reached)
    return false;

  // don't do anything when tracing is paused
  if (tdata->pause_trace_till != INVALID_ADDRESS)
    return false;

  PIN_LockClient();
  RTN ip_rtn = RTN_FindByAddress(ip);
  PIN_UnlockClient();

  if (!RTN_Valid(ip_rtn)) {
    if (AddressInGeneratedCode(tdata, ip)) {
      ossi(os);
      os << "address 0x" << std::hex << ip << " is in generated code" << std::endl;
      PRINT((os.str().c_str()));
    }
    else {
      oss(os);
      os << "can't find routine for address 0x" << std::hex << ip << std::endl;
      Exit(os, threadid);
    }

    return false;
  }

  PIN_LockClient();
  SEC from_section = RTN_Sec(RTN_FindByAddress(ip));
  PIN_UnlockClient();

  bool ignore = false;
  if (SEC_Name(from_section) == ".plt") {
    if (DEBUG_CONDITION || tdata->verbose)
    {
      // verbose output
      ossi(os);
      os << "ignored PLT-jump[" << tdata->current_call_depth << "]"
          << " " << BranchLocation(ip, taken)
          << " -> " << InstructionLocation(target)
          << std::endl;

      PRINT((os.str().c_str()));
    }

    PIN_LockClient();
    IMG target_image = IMG_FindByAddress(target);
    PIN_UnlockClient();

    /* Check whether or not tracing should be paused.
     * We do this by using a user-defined list of libraries that should not be included in the trace. */
    for (size_t idx = 0; idx < skip_libs.size(); idx++) {
      std::string x = skip_libs[idx];
      if (IMG_Name(target_image) != x)
        // no match, check next element in the list
        continue;

      ossi(os);
      os << "jump to " << InstructionLocation(target) << std::endl;
      PRINT((os.str().c_str()));
      PauseMeasurements(threadid, tdata, tdata->call_continuation_points[tdata->current_call_depth-1].continuation_point);

      // skip this call
      tdata->current_call_depth--;
      assert(tdata->current_call_depth >= 0);
      tdata->call_continuation_points[tdata->current_call_depth].continuation_point = INVALID_ADDRESS;
      tdata->call_continuation_points[tdata->current_call_depth].stack_pointer = INVALID_ADDRESS;

      // this was a match, no need to check the other items in the list
      break;
    }

    // ignore this instruction
    ignore = true;
  }

  if (!ignore)
    return GenericAtCondBranch(threadid, ip, target, INVALID_ADDRESS, taken);
  
  return false;
}

VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v) {
  oss(os);
  os << "thread begin" << std::endl;
  PRINT((os.str().c_str()));

  ThreadData* tdata = new ThreadData();

  if (PIN_SetThreadData(tls_key, tdata, threadid) == FALSE) {
    oss(os);
    os << "PIN_SetThreadData failed" << std::endl;
    Exit(os, threadid);
  }
}

static VOID dump_data(ThreadData *tdata, THREADID threadid) {
  std::ostringstream os_data_name;
  os_data_name << workdir << "/data_" << threadid << ".log";
  std::string data_name = os_data_name.str();

  std::ostringstream os_sections_name;
  os_sections_name << workdir << "/sections_" << threadid << ".log";
  std::string sections_name = os_sections_name.str();

  oss(os);
  os << "dumping data"
      << " to " << data_name
      << " and " << sections_name
      << std::endl;

  PRINT((os.str().c_str()));

  // 1/2: loaded images
  std::ofstream f_sections;
  f_sections.open(sections_name.c_str());
  if (!f_sections.is_open()) {
    oss(os);
    os << "ERROR: could not open file " << sections_name << " for writing" << std::endl;
    Exit(os, threadid);
  }

  f_sections << "# START" << std::endl;

  // loaded images (std::endl is already included)
  if (threadid == 0) {
    for (auto x : global_loads)
      f_sections << x;
  }

  for (auto x : tdata->loads)
    f_sections << x;

  f_sections << "# END" << std::endl;
  f_sections.close();

  // 2/2: collected data
  std::ofstream f_data;
  f_data.open(data_name.c_str());
  if (!f_data.is_open()) {
    oss(os);
    os << "ERROR: could not open file " << data_name << " for writing" << std::endl;
    Exit(os, threadid);
  }

  f_data << "# START" << std::endl;

  // data
  for (auto p : tdata->data) {
    BOOL taken = false;
    ADDRINT type = 0;
    ADDRINT jump = DecodeJump(p.first, taken, type);

    for (auto q : p.second) {
      BOOL prev_taken = false;
      ADDRINT prev_type = 0;
      ADDRINT prev_jump = DecodeJump(q.first, prev_taken, prev_type);

      uint64_t count = q.second;

      f_data
        << reinterpret_cast<void*>(jump)
        << "," << taken
        << "," << type
        << "," << reinterpret_cast<void*>(prev_jump)
        << "," << prev_taken
        << "," << prev_type
        << "," << count
        << std::endl;
    }
  }

  f_data << "# END" << std::endl;
  f_data.close();
}

VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 code, VOID *v) {
  oss(os);
  os << "thread end with code " << code << std::endl;
  PRINT((os.str().c_str()));

  ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));
  dump_data(tdata, threadid);
  delete tdata;
}

VOID AtControlFlow(THREADID threadid, ADDRINT ip, ADDRINT target, BOOL taken) {
  ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));

  ossi(os);
  os << "control-flow"
      << "[" << tdata->current_call_depth << "]"
      << " " << BranchLocation(ip, taken)
      << " -> " << InstructionLocation(target)
      << " " << disassemblies->Get(ip)
      << std::endl;
  PRINT((os.str().c_str()));

  oss(ot);
  ot << "IMPLEMENT ME: support control flow instruction" << std::endl;
  Exit(ot, threadid);
}

VOID AtInstruction(THREADID threadid, ADDRINT ip, const CONTEXT *cxt, ADDRINT next_ip) {
  ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));

  // sanity check
  if ((ip & ~MAX_ADDRESS) != 0) {
    ossi(os);
    os << "invalid address 0x" << std::hex << ip << " at " << InstructionLocation(ip) << std::endl;
    Exit(os, threadid);
  }

  // program's entry point reached?
  if (ip == entry_address) {
    ossi(os);
    os << "entry reached at " << std::hex << reinterpret_cast<void*>(entry_address)
        << ": " << InstructionLocation(entry_address)
        << std::endl;

    PRINT((os.str().c_str()));

    entry_reached = true;
  }

  // program's exit point reached?
  if (ip == exit_address) {
    StopMeasurements(threadid);
    return;
  }

  // don't do anything when the program's entry point is not reached yet
  if (!entry_reached)
    return;
  
  // unpause instruction tracing at the selected instruction
  if (tdata->pause_trace_till == ip) {
    ossi(os);
    os << "unpause trace at " << std::hex << reinterpret_cast<void*>(tdata->pause_trace_till)
        << ": " << InstructionLocation(tdata->pause_trace_till)
        << std::endl;

    PRINT((os.str().c_str()));

    tdata->pause_trace_till = INVALID_ADDRESS;
  }

  if (! tdata->full_trace) {
    if (ip == trace_start_address) {
      tdata->full_trace |= true;
      tdata->verbose |= true;

      oss(os);
      os << "T start full trace at " << InstructionLocation(ip) << std::endl;
      PRINT((os.str().c_str()));
    }
  }

  if (tdata->full_trace) {
    oss(os);
    os << "T" << ((tdata->pause_trace_till != INVALID_ADDRESS) ? " (paused)" : "")
        << " ins[" << ((tdata->pause_trace_till != INVALID_ADDRESS) ? -1 : tdata->current_call_depth) << "]"
        << " " << InstructionLocation(ip)
        << " " << disassemblies->Get(ip)
        << std::endl;

    PRINT((os.str().c_str()));
  }
}

VOID SyscallEntry(THREADID threadid, CONTEXT *cxt, SYSCALL_STANDARD std, VOID *v) {
  ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));

  ADDRINT nr = PIN_GetSyscallNumber(cxt, std);

  // some programs (Firefox, libpcre) use a JIT to generate code
  if (nr == SYS_mmap) {
    ADDRINT address = PIN_GetSyscallArgument(cxt, std, 0);
    ADDRINT length = PIN_GetSyscallArgument(cxt, std, 1);
    ADDRINT protection = PIN_GetSyscallArgument(cxt, std, 2);

    if ((protection & PROT_READ)
        && (protection & PROT_EXEC)) {
      if (DEBUG_CONDITION || tdata->verbose)
      {
        // verbose output
        ossi(os);
        os << "mmap[" << tdata->current_call_depth << "]"
            << " code 0x" << std::hex << address << "-0x" << (address+length-1) << std::endl;
        PRINT((os.str().c_str()));
      }

      if (address == 0) {
        // kernel will assign an address, need the result of this syscall
        tdata->syscall_exit_action = ThreadData::SYSCALL_EXIT_ACTION_MMAP_CODE;
        tdata->syscall_argument = length;
      }
      else {
        // keep track of the mapped regions
        tdata->code_regions[address] = address+length-1;
      }
    }
  }
  else if (nr == SYS_munmap) {
    ADDRINT address = PIN_GetSyscallArgument(cxt, std, 0);
    ADDRINT length = PIN_GetSyscallArgument(cxt, std, 1);

    if (DEBUG_CONDITION || tdata->verbose)
    {
      // verbose output
      ossi(os);
      os << "munmap[" << tdata->current_call_depth << "]"
          << " 0x" << std::hex << address << "-0x" << (address+length-1) << std::endl;
      PRINT((os.str().c_str()));
    }

    auto it = tdata->code_regions.find(address);
    if (it != tdata->code_regions.end()) {
      // found region to unmap, do some more sanity checks
      ADDRINT found_length = it->second - address + 1;
      if (found_length != length) {
        ossi(os);
        os << "ERROR: munmap regions of different length (0x" << std::hex << length << " vs " << found_length << ") at 0x" << address << std::endl;
        PRINT((os.str().c_str()));

        PIN_ExitProcess(1);
      }

      // remove this region from the list
      tdata->code_regions.erase(it);
    }
  }
#ifdef TRACK_INSTALLED_SIGNAL_HANDLERS
  else if (nr == SYS_rt_sigaction) {
    ADDRINT signum = PIN_GetSyscallArgument(cxt, std, 0);
    ADDRINT action = PIN_GetSyscallArgument(cxt, std, 1);
    ADDRINT oldaction = PIN_GetSyscallArgument(cxt, std, 2);
    ADDRINT sigsetsize = PIN_GetSyscallArgument(cxt, std, 3);

    struct sigaction *data = reinterpret_cast<struct sigaction *>(action);

    ADDRINT handler = reinterpret_cast<ADDRINT>(data->sa_handler);
    if (data->sa_handler == SIG_IGN
        || data->sa_handler == SIG_DFL) {
      // no action (SIG_IGN) or default action (SIG_DFL)
      return;
    }

    PIN_GetLock(&pinLock, threadid+1);
    signal_handler_addresses.insert(handler);
    PIN_ReleaseLock(&pinLock);

    oss(os);
    os << "sigaction called for signum " << signum << std::endl;
    Backtrace(os, cxt);
    PRINT((os.str().c_str()));

    oss(ot);
    // ot << std::hex << "arg0=0x" << arg0 << ", arg1=0x" << arg1 << ", arg2=0x" << arg2 << std::endl;

    ot << std::hex << "sa_handler=0x" << handler << "(loc: " << InstructionLocation(handler) << "), sa_sigaction=0x" << data->sa_sigaction << ", sa_flags=0x" << data->sa_flags << ", sa_restorer=0x" << data->sa_restorer << std::endl;
    PRINT((ot.str().c_str()));
  }
#endif
}

VOID SyscallExit(THREADID threadid, CONTEXT *cxt, SYSCALL_STANDARD std, VOID *v) {
  ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));

  switch (tdata->syscall_exit_action) {
  case ThreadData::SYSCALL_EXIT_ACTION_NONE:
    // first case for speed, no need to do anything
    return;

  case ThreadData::SYSCALL_EXIT_ACTION_MMAP_CODE: {
    ADDRINT address = PIN_GetSyscallReturn(cxt, std);
    ADDRINT length = tdata->syscall_argument;

    if (DEBUG_CONDITION || tdata->verbose) {
      ossi(os);
      os << "recording mmap-ed region assigned by kernel 0x" << std::hex << address << "-0x" << (address+length-1) << std::endl;
      PRINT((os.str().c_str()));
    }

    tdata->code_regions[address] = address+length-1;
  } break;

  default: {
    oss(os);
    os << "ERROR: unhandled syscall action " << tdata->syscall_exit_action << std::endl;
    PRINT((os.str().c_str()));
  }
  }

  tdata->syscall_exit_action = ThreadData::SYSCALL_EXIT_ACTION_NONE;
}

BOOL GenericAtCondReturn(THREADID threadid, const CONTEXT *cxt, ADDRINT ip, ADDRINT target, ADDRINT fallthrough, BOOL taken) {
  ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));

  // don't do anything when the program's entry point is not reached yet
  if (!entry_reached || exit_reached)
    return false;

  // don't do anything when tracing is paused
  if (tdata->pause_trace_till != INVALID_ADDRESS)
    return false;

  if (DEBUG_CONDITION || tdata->verbose) {
    // verbose output
    ossi(os);
    os << ((fallthrough == INVALID_ADDRESS) ? "un" : "") << "cond-ret"
        << "[" << tdata->current_call_depth << "]"
        << " " << BranchLocation(ip, taken)
        << " -> " << InstructionLocation(target)
        << std::endl;

    PRINT((os.str().c_str()));
  }

  /* Don't do any bookkeeping for the program's RETURN instruction that
   * terminates the program (i.e., returns to __libc_start_main). */
  if (target == exit_address)
    return false;

  BOOL result = false;

  if (fallthrough == INVALID_ADDRESS) {
    // unconditional returns
    tdata->current_call_depth--;

    if (tdata->current_call_depth < 0) {
      oss(os);
      os << "current call depth < 0"
          << std::endl;
      Exit(os, threadid);
    }

    // sanity check
    assert(tdata->current_call_depth >= 0);

    bool clear_entry = true;
    if (target != tdata->call_continuation_points[tdata->current_call_depth].continuation_point) {
      // not going to the expected return site
      int new_call_depth = -1;

      // maybe we return to __rt_restore?
      if (target == address_of_restore_rt) {
        ossi(os);
        os << InstructionLocation(target) << " returns to __restore_rt, taking care of this!" << std::endl;
        PRINT((os.str().c_str()));

        clear_entry = false;
        tdata->current_call_depth++;
      }
      else {
        // first try to find a corresponding call site based on the stack pointer
        ADDRINT stack_pointer = PIN_GetContextReg(cxt, REG_STACK_PTR);
        for (int i = tdata->current_call_depth; i >= 0; i--) {
          if (tdata->call_continuation_points[i].stack_pointer == stack_pointer) {
            // found
            new_call_depth = i;
            break;
          }
        }

        if (new_call_depth == -1) {
          // not found: problem!
          oss(os);
          os << "ERROR: call returns to unexpected continuation point" << std::endl;
          os << "       actual address  :"
              << " " << InstructionLocation(target)
              << " (" << std::hex << reinterpret_cast<void*>(PIN_GetContextReg(cxt, REG_STACK_PTR)) << ")"
              << std::endl;
          os << "       expected address: ???" << std::endl;
          os << "Return instruction: " << InstructionLocation(ip) << " " << disassemblies->Get(ip) << std::endl;

          Backtrace(os, cxt);

          Exit(os, threadid);
        }
        else if (target != tdata->call_continuation_points[new_call_depth].continuation_point) {
          // found, but unexpected continuation point: problem!
          oss(os);
          os << "ERROR: call returns to unexpected continuation point" << std::endl;
          os << "       actual address  :"
              << " " << InstructionLocation(target)
              << " (" << std::hex << reinterpret_cast<void*>(PIN_GetContextReg(cxt, REG_STACK_PTR)) << ")"
              << std::endl;
          os << "       expected address:"
              << " " << InstructionLocation(tdata->call_continuation_points[new_call_depth].continuation_point)
              << " (" << std::hex << reinterpret_cast<void*>(tdata->call_continuation_points[new_call_depth].stack_pointer) << ")"
              << std::endl;
          os << "Return instruction: " << InstructionLocation(ip) << " " << disassemblies->Get(ip) << std::endl;

          Backtrace(os, cxt);

          Exit(os, threadid);
        }
        else {
          // found, and correct continuation point: no problem!
          for (int i = tdata->current_call_depth; i > new_call_depth; i--) {
            tdata->call_continuation_points[i].continuation_point = INVALID_ADDRESS;
            tdata->call_continuation_points[i].stack_pointer = INVALID_ADDRESS;
          }

          tdata->current_call_depth = new_call_depth;
        }
      }
    }

    if (clear_entry) {
      // sanity check
      assert(target == tdata->call_continuation_points[tdata->current_call_depth].continuation_point);

      tdata->call_continuation_points[tdata->current_call_depth].continuation_point = INVALID_ADDRESS;
      tdata->call_continuation_points[tdata->current_call_depth].stack_pointer = INVALID_ADDRESS;

      result = true;
    }
  }
  else {
    // conditional returns
    oss(os);
    os << "IMPLEMENT ME: support for conditional return instructions" << std::endl;
    Exit(os, threadid);
  }

  return result;
}

BOOL GenericAtReturn(THREADID threadid, const CONTEXT *cxt, ADDRINT ip, ADDRINT target, BOOL taken) {
  return GenericAtCondReturn(threadid, cxt, ip, target, INVALID_ADDRESS, taken);
}

VOID AtSyscall(THREADID threadid, const CONTEXT *cxt, ADDRINT ip) {
  ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));

  // don't do anything when the program's entry point is not reached yet
  if (!entry_reached || exit_reached)
    return;

  // don't do anything when tracing is paused
  if (tdata->pause_trace_till != INVALID_ADDRESS)
    return;

  ADDRINT nr = PIN_GetSyscallNumber(cxt, SYSCALL_STANDARD_IA32E_LINUX);

  if (DEBUG_CONDITION || tdata->verbose)
  {
    // verbose output
    ossi(os);
    os << "syscall"
        << "[" << tdata->current_call_depth << "]"
        << " " << BranchLocation(ip, true)
        << " -> " << nr
        << std::endl;
  }
}

BOOL GenericAtCondCall(THREADID threadid, const CONTEXT *cxt, ADDRINT ip, ADDRINT target, ADDRINT fallthrough, BOOL taken, ADDRINT next_ip) {
  ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));

  /* We want to know when to stop collecting statistics,
   * which we do when the program exits. */
  if (!entry_reached
      && (target == entry_address)) {
    ossi(os);
    os << "calling program entry"
        << " " << ((fallthrough == INVALID_ADDRESS) ? "un" : "") << "cond-call"
        << "[" << tdata->current_call_depth << "]"
        << " " << BranchLocation(ip, taken)
        << " -> " << InstructionLocation(target)
        << std::endl;
    PRINT((os.str().c_str()));

    // we know the address of the first instruction that will be executed after the program exits.
    exit_address = next_ip;

    ossi(ot);
    ot << "address of first instruction after program exit is"
        << " 0x" << std::hex << exit_address
        << ": " << InstructionLocation(exit_address) << std::endl;
    PRINT((ot.str().c_str()));
  }

  // don't do anything when the program's entry point is not reached yet
  if (!entry_reached || exit_reached)
    return false;

  // don't do anything when tracing is paused
  if (tdata->pause_trace_till != INVALID_ADDRESS)
    return false;

  // call to generated code?
  PIN_LockClient();
  RTN target_rtn = RTN_FindByAddress(target);
  PIN_UnlockClient();

  if (!RTN_Valid(target_rtn)) {
    if (AddressInGeneratedCode(tdata, target)) {
      ossi(os);
      os << InstructionLocation(ip) << ": calling 0x" << std::hex << target << ", which is in generated code" << std::endl;
      PRINT((os.str().c_str()));

      PauseMeasurements(threadid, tdata, next_ip);
    }
    else {
      oss(os);
      os << "can't find callee for address 0x" << target << std::endl;
      Exit(os, threadid);
    }

    return false;
  }

  if (DEBUG_CONDITION || tdata->verbose) {
    // verbose output
    ossi(os);
    os << ((fallthrough == INVALID_ADDRESS) ? "un" : "") << "cond-call"
        << "[" << tdata->current_call_depth << "]"
        << " " << BranchLocation(ip, taken)
        << " -> " << InstructionLocation(target)
        << std::endl;

    PRINT((os.str().c_str()));
  }

  BOOL result = false;

  if (fallthrough == INVALID_ADDRESS) {
    // unconditional calls
    tdata->call_continuation_points[tdata->current_call_depth].continuation_point = next_ip;
    tdata->call_continuation_points[tdata->current_call_depth].stack_pointer = PIN_GetContextReg(cxt, REG_STACK_PTR) - 8;

    tdata->current_call_depth++;
    assert(tdata->current_call_depth < MAX_CALL_DEPTH);

    result = true;
  }
  else {
    // conditional calls
    oss(os);
    os << "IMPLEMENT ME: support for conditional call instructions" << std::endl;
    Exit(os, threadid);
  }

  return result;
}

BOOL GenericAtCall(THREADID threadid, const CONTEXT *cxt, ADDRINT ip, ADDRINT target, BOOL taken, ADDRINT next_ip) {
  return GenericAtCondCall(threadid, cxt, ip, target, INVALID_ADDRESS, taken, next_ip);
}

std::string GetWorkDirectory() {
  char work_directory[2048];

  // directory in which output files should be stored
  if (!getcwd(work_directory, sizeof(work_directory))) {
    printf("ERROR: could not get current directory\n");
    Exit();
  }

  return std::string(work_directory);
}
