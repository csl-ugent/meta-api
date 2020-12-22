#include <map>
#include "pin.H"

#include "disasm_container.h"

#define DEBUG_CONDITION false

#define USED __attribute__((used))

// #define FLUSH fflush(stdout)
#define FLUSH

#define PRINT(x) do {\
  PIN_GetLock(&pinLock, threadid+1); \
  printf x ; \
  FLUSH ; \
  PIN_ReleaseLock(&pinLock); \
} while(0)

/* Do this in a macro because the PIN CRT does not support C++11,
 * where we can return streams from functions (std::move). */
#define oss(name) \
    std::ostringstream name; \
    do { \
      name << "[" << threadid << "] "; \
    } while (0)
#define ossi(name) \
    std::ostringstream name; \
    do { \
      IndentedOSS(name, tdata->current_call_depth); \
      name << "[" << threadid << "] "; \
    } while (0)

static const int MAX_CALL_DEPTH = 4096;

// Max address of instructions in user space.
// Upper bits can be (ab)used to store metadata.
static const ADDRINT MAX_ADDRESS = 0x00007fffffffffffULL;

static const ADDRINT INVALID_DECODED_ADDRESS = 0x7fffffffffffffffULL;
static const ADDRINT INVALID_ADDRESS = -1ULL;

// Metadata: is the branch taken or not?
static const ADDRINT TAKEN_BIT = 1ULL << 63;

// Metadata: type of branch
static const ADDRINT TYPE_BIT = 56;
static const ADDRINT TYPE_MASK = 0xfULL << TYPE_BIT;
// IMPORTANT: if you modify these flags, change the measurement Python scripts accordingly!
static const ADDRINT CfBranch      = 1ULL << TYPE_BIT; //0001b
static const ADDRINT CfCall        = 3ULL << TYPE_BIT; //0010b
static const ADDRINT CfReturn      = 6ULL << TYPE_BIT; //0011b
static const ADDRINT CfConditional = 4ULL << TYPE_BIT; //0100b
static const ADDRINT CfDirect      = 8ULL << TYPE_BIT; //1000b

// custom types
typedef std::map<ADDRINT, uint64_t> t_addr_to_counter;

// thread-specific data management
extern PIN_LOCK pinLock;
extern TLS_KEY tls_key;

extern DISASM_CONTAINER *disassemblies;

extern std::string workdir;

extern std::vector<std::string> skip_libs;

// thread-specific data structure
struct ThreadData {
  std::map<ADDRINT, t_addr_to_counter> data;
  int current_call_depth;
  std::vector<ADDRINT> prev_jumps;
  ADDRINT pause_trace_till;
  bool full_trace;
  bool verbose;
  std::vector<std::string> loads;
  std::map<ADDRINT, ADDRINT> code_regions;

  enum SyscallExitAction {
    SYSCALL_EXIT_ACTION_NONE,
    SYSCALL_EXIT_ACTION_MMAP_CODE
  };
  SyscallExitAction syscall_exit_action;
  ADDRINT syscall_argument;

  struct ContinuationAddress {
    ADDRINT continuation_point;
    ADDRINT stack_pointer;

    ContinuationAddress() {
      continuation_point = INVALID_ADDRESS;
      stack_pointer = INVALID_ADDRESS;
    }
  };
  std::vector<ContinuationAddress> call_continuation_points;

  ThreadData() {
    current_call_depth = 0;
    prev_jumps = std::vector<ADDRINT>(MAX_CALL_DEPTH, INVALID_ADDRESS);
    call_continuation_points = std::vector<ContinuationAddress>(MAX_CALL_DEPTH, ContinuationAddress());
    pause_trace_till = INVALID_ADDRESS;
    full_trace = false;
    verbose = false;
    syscall_exit_action = SYSCALL_EXIT_ACTION_NONE;
  }
};

// exit
VOID Exit();
VOID Exit(std::ostringstream& os, THREADID threadid);

// backtrace, calltrace
VOID Backtrace(std::ostringstream& os, const CONTEXT *cxt);
VOID Calltrace(std::ostringstream& os, ThreadData *tdata);

// measurements
VOID StopMeasurements(THREADID threadid);
VOID PauseMeasurements(THREADID threadid, ThreadData *tdata, ADDRINT resume_address);

// printers
std::string InstructionLocation(ADDRINT ins);
std::string BranchLocation(ADDRINT address, BOOL taken);
VOID IndentedOSS(std::ostringstream& os, int depth);

// instrumenters
VOID ImageLoad(IMG img, VOID *v);
VOID ImageUnload(IMG img, VOID *v);

VOID ThreadStart(THREADID threadid, CONTEXT *ctxt, INT32 flags, VOID *v);
VOID ThreadFini(THREADID threadid, const CONTEXT *ctxt, INT32 code, VOID *v);

BOOL GenericAtBranch(THREADID threadid, ADDRINT ip, ADDRINT target, BOOL taken);
BOOL GenericAtCondBranch(THREADID threadid, ADDRINT ip, ADDRINT target, ADDRINT fallthrough, BOOL taken);

BOOL GenericAtCall(THREADID threadid, const CONTEXT *cxt, ADDRINT ip, ADDRINT target, BOOL taken, ADDRINT next_ip);
BOOL GenericAtCondCall(THREADID threadid, const CONTEXT *cxt, ADDRINT ip, ADDRINT target, ADDRINT fallthrough, BOOL taken, ADDRINT next_ip);

BOOL GenericAtReturn(THREADID threadid, const CONTEXT *cxt, ADDRINT ip, ADDRINT target, BOOL taken);
BOOL GenericAtCondReturn(THREADID threadid, const CONTEXT *cxt, ADDRINT ip, ADDRINT target, ADDRINT fallthrough, BOOL taken);

VOID AtControlFlow(THREADID threadid, ADDRINT ip, ADDRINT target, BOOL taken);
VOID AtInstruction(THREADID threadid, ADDRINT ip, const CONTEXT *cxt, ADDRINT next_ip);

VOID AtSyscall(THREADID threadid, const CONTEXT *cxt, ADDRINT ip);
VOID SyscallExit(THREADID threadid, CONTEXT *cxt, SYSCALL_STANDARD std, VOID *v);
VOID SyscallEntry(THREADID threadid, CONTEXT *cxt, SYSCALL_STANDARD std, VOID *v);

// helpers
ADDRINT DecodeJump(ADDRINT encoded, BOOL& taken, ADDRINT& type);
ADDRINT EncodeJump(ADDRINT address, BOOL taken);
BOOL AddressInGeneratedCode(ThreadData* tdata, ADDRINT address);
VOID LibcStartMainCalled(THREADID threadid, ADDRINT address_of_main);
std::string GetWorkDirectory();
