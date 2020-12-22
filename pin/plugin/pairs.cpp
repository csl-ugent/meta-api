#include "common.h"

// getcwd
#include <unistd.h>

static VOID Record(THREADID threadid, ThreadData* tdata, ADDRINT ip, ADDRINT target, ADDRINT fallthrough, BOOL taken, uint64_t type) {
  ADDRINT next_address = taken ? target : fallthrough;
  if (!taken
      && (next_address == INVALID_ADDRESS)) {
    oss(os);
    os << "control flow instruction is marked as not-taken, but has no valid fallthrough address" << std::endl
        << " " << BranchLocation(ip, taken) << std::endl;
    Exit(os, threadid);
  }

  assert(sizeof(ADDRINT) == 8);
  ADDRINT encoded_jump = EncodeJump(ip, taken) | type;

  // make sure we can store the data
  if (tdata->data.find(encoded_jump) == tdata->data.end())
    tdata->data[encoded_jump] = t_addr_to_counter();

  // take note of this instruction pair
  if (tdata->data[encoded_jump].find(next_address) == tdata->data[encoded_jump].end())
    tdata->data[encoded_jump][next_address] = 1;
  else
    tdata->data[encoded_jump][next_address]++;
}

static VOID AtCondBranch(THREADID threadid, ADDRINT ip, ADDRINT target, ADDRINT fallthrough, BOOL taken, BOOL direct) {
  BOOL do_record = GenericAtCondBranch(threadid, ip, target, fallthrough, taken);
  if (do_record) {
    ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));
    Record(threadid, tdata, ip, target, fallthrough, taken, CfBranch | CfConditional | (direct ? CfDirect : 0));
  }
}

static void AtBranch(THREADID threadid, ADDRINT ip, ADDRINT target, BOOL taken, BOOL direct) {
  BOOL do_record = GenericAtBranch(threadid, ip, target, taken);
  if (do_record) {
    ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));
    Record(threadid, tdata, ip, target, INVALID_ADDRESS, taken, CfBranch | (direct ? CfDirect : 0));
  }
}

static VOID AtCondCall(THREADID threadid, const CONTEXT *cxt, ADDRINT ip, ADDRINT target, ADDRINT fallthrough, BOOL taken, ADDRINT next_ip, BOOL direct) {
  BOOL do_record = GenericAtCondCall(threadid, cxt, ip, target, fallthrough, taken, next_ip);
  if (do_record) {
    ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));
    Record(threadid, tdata, ip, target, fallthrough, taken, CfCall | CfConditional | (direct ? CfDirect : 0));
  }
}

static VOID AtCall(THREADID threadid, const CONTEXT *cxt, ADDRINT ip, ADDRINT target, BOOL taken, ADDRINT next_ip, BOOL direct) {
  BOOL do_record = GenericAtCall(threadid, cxt, ip, target, taken, next_ip);
  if (do_record) {
    ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));
    Record(threadid, tdata, ip, target, INVALID_ADDRESS, taken, CfCall | (direct ? CfDirect : 0));
  }
}

static VOID AtCondReturn(THREADID threadid, const CONTEXT *cxt, ADDRINT ip, ADDRINT target, ADDRINT fallthrough, BOOL taken) {
  BOOL do_record = GenericAtCondReturn(threadid, cxt, ip, target, fallthrough, taken);
  if (do_record) {
    ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));
    Record(threadid, tdata, ip, target, fallthrough, taken, CfReturn | CfConditional);
  }
}

static VOID AtReturn(THREADID threadid, const CONTEXT *cxt, ADDRINT ip, ADDRINT target, BOOL taken) {
  BOOL do_record = GenericAtReturn(threadid, cxt, ip, target, taken);
  if (do_record) {
    ThreadData* tdata = static_cast<ThreadData*>(PIN_GetThreadData(tls_key, threadid));
    Record(threadid, tdata, ip, target, INVALID_ADDRESS, taken, CfReturn);
  }
}

static VOID Fini(INT32 code, VOID *v)
{
  // nothing to do
}

static VOID Instruction(INS ins, VOID *v)
{
  disassemblies->Add(INS_Address(ins), INS_Disassemble(ins));

  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtInstruction, IARG_THREAD_ID, IARG_INST_PTR, IARG_CONST_CONTEXT, IARG_ADDRINT, INS_NextAddress(ins), IARG_END);

  if (INS_IsBranch(ins)) {
    // (conditional) branch instructions
    if (INS_HasFallThrough(ins)) {
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtCondBranch, IARG_THREAD_ID, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_FALLTHROUGH_ADDR, IARG_BRANCH_TAKEN, IARG_BOOL, INS_IsDirectBranch(ins), IARG_END);
    }
    else {
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtBranch, IARG_THREAD_ID, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_BOOL, INS_IsDirectBranch(ins), IARG_END);
    }
  }
  else if (INS_IsCall(ins)) {
    // (conditional) call instructions
    if (INS_HasFallThrough(ins)) {
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtCondCall, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_FALLTHROUGH_ADDR, IARG_BRANCH_TAKEN, IARG_ADDRINT, INS_NextAddress(ins), IARG_BOOL, INS_IsDirectCall(ins), IARG_END);
    }
    else {
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtCall, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_ADDRINT, INS_NextAddress(ins), IARG_BOOL, INS_IsDirectCall(ins), IARG_END);
    }
  }
  else if (INS_IsRet(ins)) {
    // (conditional) return instructions
    if (INS_HasFallThrough(ins))
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtCondReturn, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_FALLTHROUGH_ADDR, IARG_BRANCH_TAKEN, IARG_END);
    else
      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtReturn, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_END);
  }
  else if (INS_IsSyscall(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtSyscall, IARG_THREAD_ID, IARG_CONST_CONTEXT, IARG_INST_PTR, IARG_ADDRINT, INS_NextAddress(ins), IARG_END);
  }
  else if (INS_IsControlFlow(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)AtControlFlow, IARG_THREAD_ID, IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_BRANCH_TAKEN, IARG_END);
  }
}

int main(INT32 argc, CHAR **argv)
{
  // Initialize the spin lock
  PIN_InitLock(&pinLock);

  PIN_InitSymbols();

  PIN_Init(argc, argv);

  // to emit the output files
  workdir = GetWorkDirectory();

  // to cache the disassemblies
  disassemblies = new DISASM_CONTAINER();

  // obtain a key for TLS storage
  tls_key = PIN_CreateThreadDataKey(NULL);
  if (tls_key == INVALID_TLS_KEY) {
    printf("ERROR: number of already allocated keys reached the MAX_CLIENT_TLS_KEYS limit\n");
    Exit();
  }

  // instruction tracing
  INS_AddInstrumentFunction(Instruction, NULL);

  // image (un)loading
  IMG_AddInstrumentFunction(ImageLoad, NULL);
  IMG_AddUnloadFunction(ImageUnload, NULL);

  // thread management
  PIN_AddThreadStartFunction(ThreadStart, NULL);
  PIN_AddThreadFiniFunction(ThreadFini, NULL);

  // syscall handling
  PIN_AddSyscallEntryFunction(SyscallEntry, NULL);
  PIN_AddSyscallExitFunction(SyscallExit, NULL);

  // finalization
  PIN_AddFiniFunction(Fini, 0);

  // libraries not to be measured
  // skip_libs.push_back("/lib/x86_64-linux-gnu/libdl.so.2");

  // this call never returns
  PIN_StartProgram();

  return 0;
}
