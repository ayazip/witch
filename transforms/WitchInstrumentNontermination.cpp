//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.

#include <map>
#include <vector>
#include <set>

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/IR/DebugInfoMetadata.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"

extern llvm::cl::opt<bool> insertHeader;

using namespace llvm;

bool CloneMetadata(const llvm::Instruction *, llvm::Instruction *);

class WitchInstrumentNontermination : public LoopPass {
  bool checkInstruction(Instruction& I, std::set<llvm::Value *>& variables,
                        std::vector<llvm::Function *> callstack);
  bool checkFunction(Function *F, std::set<llvm::Value *>& variables,
                     std::vector<llvm::Function *> callstack);
  bool instrumentLoop(Loop *L);
  bool instrumentLoop(Loop *L, const std::set<llvm::Value *>& variables);
  bool instrumentEmptyLoop(Loop *L);

  bool checkOperand(llvm::Value *v,
                    std::set<llvm::Value *>& usedValues,
                    bool nestedCall) {
      if (isa<AllocaInst>(v)) {
          if (!nestedCall) {
            usedValues.insert(v); // we do not care about allocas from nested calls
          }
          return true;
      } else if (isa<GlobalVariable>(v)) {
          usedValues.insert(v);
          return true;
      }
      // check only after global, global is also constant
      if (isa<Constant>(v))
          return true;

      return false;
  }

  Function *_assert{nullptr};
  Function *_fail{nullptr};
  Function *_header{nullptr};
  Function *_store{nullptr};
  Function *_nondet{nullptr};


  Function *getHeaderFun(Module *M) {
    if (!_header) {
      auto& Ctx = M->getContext();
      auto F = M->getOrInsertFunction("__INSTR_check_nontermination_header",
                                      Type::getVoidTy(Ctx) // retval
#if LLVM_VERSION_MAJOR < 5
                                      , nullptr
#endif
                                      );
#if LLVM_VERSION_MAJOR >= 9
      _header = cast<Function>(F.getCallee()->stripPointerCasts());
#else
      _header = cast<Function>(F);
#endif
    }
    return _header;
  }

  Function *getStoreFun(Module *M) {
      if (!_store) {
          auto& Ctx = M->getContext();
          auto F = M->getOrInsertFunction("__INSTR_store",
                                          Type::getVoidTy(Ctx) // retval
                                          );
          _store = cast<Function>(F.getCallee()->stripPointerCasts());
      }
      return _store;
  }

  Function *getNondetStore(Module *M) {
      if (!_nondet) {
          auto& Ctx = M->getContext();
          auto F = M->getOrInsertFunction("__INSTR_nondet_store",
                                          Type::getInt1Ty(Ctx) // retval
                                          );
          _nondet = cast<Function>(F.getCallee()->stripPointerCasts());
      }
      return _nondet;
  }

  Function *getAssert(Module *M) {
      if (!_assert) {
          auto& Ctx = M->getContext();
          auto F = M->getOrInsertFunction("__INSTR_check_nontermination",
                                          Type::getVoidTy(Ctx), // retval
                                          Type::getInt1Ty(Ctx)  // condition
                                          );
          _assert = cast<Function>(F.getCallee()->stripPointerCasts());
      }
      return _assert;
  }


  public:
    static char ID;

    WitchInstrumentNontermination() : LoopPass(ID) {}

    bool runOnLoop(Loop *L, LPPassManager & /*LPM*/) override {
      // for now, we detect only nested loops
      if (L->getParentLoop()) {
          // run on non-nested loops for now
          return false;
      }

      return instrumentLoop(L);
    }
};

bool WitchInstrumentNontermination::checkFunction(Function *F,
                                             std::set<llvm::Value *>& usedValues,
                                             std::vector<llvm::Function *> callstack) {
  if (!F) // call via pointer
      return false;

  if (F->getName().equals("__VERIFIER_assume") ||
      F->getName().equals("__VERIFIER_assert") ||
      F->getName().startswith("__VERIFIER_nondet_") ||
      F->getName().startswith("__VERIFIER_exit") ||
      F->getName().startswith("__VERIFIER_silent_exit") ||
      F->getName().startswith("exit") ||
      F->getName().startswith("_exit") ||
      F->getName().startswith("abort") ||
      F->getName().startswith("klee_silent_exit") ||
      F->getName().startswith("llvm.dbg.") ||
      F->getName().startswith("__VALIDATOR") ||
      F->getName().equals("__INSTR_store"))
    return true;

  for (auto *onstack : callstack) {
      if (onstack == F) {
          return false; // recursion
      }
  }

  callstack.push_back(F);

  for (auto& B : *F) {
    for (auto& I : B) {
      if (!checkInstruction(I, usedValues, callstack)) {
        return false;
      }
    }
  }

  return true;
}

bool WitchInstrumentNontermination::instrumentLoop(Loop *L) {
  std::set<llvm::Value *> usedValues;

  for (auto *block : L->blocks()) {
    // check that the loop reads and writes only to known
    // locations (allocas and global variables)
    for (auto& I : *block) {
      // hmm... could be implemented more efficiently,
      // but it should be quite fast even though.
      if (!checkInstruction(I, usedValues, {})) {
        return false;
      }
    }
  }

  // all ok
  return instrumentLoop(L, usedValues);
}

bool WitchInstrumentNontermination::checkInstruction(Instruction& I,
                                                std::set<llvm::Value*>& usedValues,
                                                std::vector<llvm::Function *> callstack) {
  bool isNested = !callstack.empty();
  //llvm::errs() << "checking (" << isNested << "): " << I << "\n";

  if (auto *CI = dyn_cast<CallInst>(&I)) {
    if (!checkFunction(CI->getCalledFunction(), usedValues, callstack)) {
      return false;
    }
  } else if (auto LI = dyn_cast<LoadInst>(&I)) {
    if (!checkOperand(LI->getPointerOperand(), usedValues, isNested)) {
      return false;
    }
  } else if (auto SI = dyn_cast<StoreInst>(&I)) {
    if (!checkOperand(SI->getPointerOperand(), usedValues, isNested)) {
      return false;
    }
  } else {
    if (I.mayReadOrWriteMemory()) {
      llvm::errs() << "WARNING: Unhandled instr: " << I << "\n";
      return false;
    }
  }

  return true;
}


bool WitchInstrumentNontermination::instrumentLoop(Loop *L, const std::set<llvm::Value *>& variables) {
  auto *header = L->getHeader();
  assert(header);
  auto *M = header->getModule();
  LLVMContext& Ctx = M->getContext();

  // Remeber the original predecessors of the header
  std::vector<std::pair<BasicBlock *, unsigned>> to_change;
  for (auto I = pred_begin(header), E = pred_end(header); I != E; ++I) {
      auto TI = (*I)->getTerminator();
      for (int i = 0, e = TI->getNumSuccessors(); i < e; ++i) {
          if (TI->getSuccessor(i) == header)
              to_change.emplace_back(*I, i);
      }
  }

  // mapping of old to new ones
  std::map<Value *, Value *> mapping;

  // for each variable, create its copy in the header
  // and store the last recent value from the original
  // variable
  for (auto *v : variables) {
    //errs() << "INFO: variable: " << *v << "\n";
    Instruction *newVal = nullptr;
    if (auto *I = dyn_cast<Instruction>(v)) {
        newVal = I->clone();
        newVal->insertAfter(I);
    } else if (auto *G = dyn_cast<GlobalValue>(v)) {
        // create a new alloca that
        // is going to be inserted at the beginning of the header
        newVal = new AllocaInst(
            G->getType()->getContainedType(0),
            G->getType()->getAddressSpace(),
            nullptr,
            "",
            // put the alloca on the beginning of the function
            header->getParent()->getBasicBlockList().front().getTerminator());
    } else {
      llvm::errs() << "ERROR: Unhandled copying: " << *v << "\n";
      return false;
    }

    assert(newVal);
    mapping[v] = newVal;
  }

  if (mapping.empty()) {
      return instrumentEmptyLoop(L);
  }

  // Create a new BB, where we call __INSTR_store and decide whether
  // to save the current values
  BasicBlock *decideStore = BasicBlock::Create(Ctx, "decide.store");
  BasicBlock *storeBlock = BasicBlock::Create(Ctx, "store.values");

  // insert the new blocks before header
  storeBlock->insertInto(header->getParent(), header);
  decideStore->insertInto(header->getParent(), storeBlock);

  auto *nondetStore = CallInst::Create(getNondetStore(M));
  decideStore->getInstList().push_back(nondetStore);

  BranchInst::Create(storeBlock, header, nondetStore, decideStore);

  // store the state of variables at the loop head
  auto *storeFun = CallInst::Create(getStoreFun(M));
  storeBlock->getInstList().push_back(storeFun);

  for (auto& it : mapping) {
    auto *LI = new LoadInst(
        it.first->getType()->getPointerElementType(),
        it.first,
        "",
        false,
        M->getDataLayout().getABITypeAlign(it.first->getType()),
        static_cast<Instruction*>(nullptr));

    auto *SI = new StoreInst(LI,
        it.second,
        false,
        LI->getAlign(),
        static_cast<Instruction*>(nullptr));

    storeBlock->getInstList().push_back(LI);
    storeBlock->getInstList().push_back(SI);
  }

  BranchInst::Create(header, storeBlock);

  if (insertHeader) {
      auto *CI = CallInst::Create(getHeaderFun(M));
      // copy the location from terminator, so that we have
      // the right debug loc
      CloneMetadata(decideStore->getFirstNonPHIOrDbg(), CI);
      CI->insertBefore(decideStore->getFirstNonPHIOrDbg());
  }

  // compare the old and new values after the iteration of the loop
  for (auto I = pred_begin(header), E = pred_end(header); I != E; ++I) {
    auto *term = (*I)->getTerminator();

    // the state must be stored before any enter ofer,
    // but the assertions are inserted only before the
    // jumps that come from the loop
    if (!L->contains(*I))
      continue;

    // create an assertion that the values are not all the same
    // as the old values (if this assert fails, we found
    // a cycle in the state space)
    Instruction *lastCond = nullptr;
    for (auto& it : mapping) {
      auto *newVal = new LoadInst(
          it.first->getType()->getPointerElementType(),
          it.first,
          "",
          term);
      auto *oldVal = new LoadInst(
          it.second->getType()->getPointerElementType(),
          it.second,
          "",
          term);
      auto *cmp = new ICmpInst(ICmpInst::ICMP_EQ, newVal, oldVal);

      auto md = term->getPrevNonDebugInstruction();
      if (!md || !md->hasMetadata())
          md = term;

      CloneMetadata(md, newVal);
      CloneMetadata(md, oldVal);
      CloneMetadata(md, cmp);
      cmp->insertBefore(term);

      if (lastCond) {
        assert(mapping.size() > 1); // we can get here only after 1 iteration
        auto *And = BinaryOperator::Create(Instruction::And, lastCond, cmp);
        And->insertBefore(term);
        lastCond = And;
      } else {
        lastCond = cmp;
      }
    }

    assert(lastCond);

    // insert the assertion that all the values are the same
    auto *CI = CallInst::Create(getAssert(M), {lastCond});
    if (lastCond->hasMetadata())
      CloneMetadata(lastCond, CI);
    else
      CloneMetadata(term, CI);
    CI->insertBefore(term);
  }

  // now change the jump instructions
  for (auto& pr : to_change) {
      auto TI = pr.first->getTerminator();
      TI->setSuccessor(pr.second, decideStore);
  }

  L->addBlockEntry(decideStore);
  L->addBlockEntry(storeBlock);

  L->moveToHeader(decideStore);


  llvm::errs() << "Instrumented a loop with non-termination checks\n";
  return true;
}

bool WitchInstrumentNontermination::instrumentEmptyLoop(Loop *L) {
  auto *header = L->getHeader();

  // go after unique successors and if you get to a loop,
  // we know this loop does not terminate
  // (since it passed our checks and it does not use any
  // variables, we know it may not terminate even from
  // some call)

  // it is an infinite loop
  auto M = header->getParent()->getParent();
  auto& Ctx = M->getContext();
  if (!_fail) {
    auto F = M->getOrInsertFunction("__INSTR_infinite_loop",
                                    Type::getVoidTy(Ctx) // retval
                                    );
    _fail = cast<Function>(F.getCallee()->stripPointerCasts());
  }

  auto *CI = CallInst::Create(_fail);
  CloneMetadata(header->getFirstNonPHIOrDbg(), CI);
  CI->insertBefore(header->getFirstNonPHIOrDbg());



  llvm::errs() << "Instrumented an empty loop.\n";
  return true;
}

static RegisterPass<WitchInstrumentNontermination> CL("witch-instrument-nontermination",
                                                      "Insert trivial checks for state space cycles");
char WitchInstrumentNontermination::ID;

