
// Simple WebAssembly interpreter, designed to be embeddable in JavaScript, so it
// can function as a polyfill.

#include "wasm.h"

using namespace cashew;
using namespace wasm;

namespace wasm {

// An instance of a WebAssembly module
class ModuleInstance {
public:
  ModuleInstance(Module& wasm) : wasm(wasm) {
    for (auto function : wasm.functions) {
      functions[function->name] = function;
    }
  }

  typedef std::vector<Literal> LiteralList;

  Literal callFunction(IString name) {
    LiteralList empty;
    return callFunction(name, empty);
  }

  Literal callFunction(IString name, LiteralList& arguments) {

    class FunctionScope {
    public:
      std::map<IString, Literal> locals;

      FunctionScope(Function* function, LiteralList& arguments) {
        assert(function->params.size() == arguments.size());
        for (size_t i = 0; i < arguments.size(); i++) {
          assert(function->params[i].type == arguments[i].type);
          locals[function->params[i].name] = arguments[i];
        }
        for (auto& local : function->locals) {
          locals[local.name].type = local.type;
        }
      }
    };

    // Stuff that flows around during executing expressions: a literal, or a change in control flow
    class Flow {
    public:
      Flow() {}
      Flow(Literal value) : value(value) {}

      Literal value;
      IString breakTo; // if non-null, a break is going on

      bool breaking() { return breakTo.is(); }

      void clearIf(IString target) {
        if (breakTo == target) {
          breakTo.clear();
        }
      }
    };

    // Execute a statement
    class ExpressionRunner : public WasmVisitor<Flow> {
      FunctionScope& scope;
    public:
      ExpressionRunner(FunctionScope& scope) : scope(scope) {}

      Flow visitBlock(Block *curr) override {
        Flow flow;
        for (auto expression : curr->list) {
          flow = visit(expression);
          if (flow.breaking()) {
            flow.clearIf(curr->name);
            return flow;
          }
        }
        return flow;
      }
      Flow visitIf(If *curr) override {
        Flow flow = visit(curr->condition);
        if (flow.breaking()) return flow;
        if (flow.value.geti32()) return visit(curr->ifTrue);
        if (curr->ifFalse) return visit(curr->ifFalse);
        return Flow();
      }
      Flow visitLoop(Loop *curr) override {
        while (1) {
          Flow flow = visit(curr->body);
          if (flow.breaking()) {
            if (flow.breakTo == curr->in) continue; // lol
            flow.clearIf(curr->out);
            return flow;
          }
        }
      }
      Flow visitLabel(Label *curr) override {
        Flow flow = visit(curr->body);
        flow.clearIf(curr->name);
        return flow;
      }
      Flow visitBreak(Break *curr) override {
        if (curr->condition) {
          Flow flow = visit(curr->condition);
          if (flow.breaking()) return flow;
          if (!flow.value.geti32()) return Flow();
        }
        Flow flow = visit(curr->value);
        if (!flow.breaking()) {
          flow.breakTo = curr->name;
        }
        return flow;
      }
      Flow visitSwitch(Switch *curr) override {
        abort();
      }
      Flow visitCall(Call *curr) override {
        LiteralList arguments;
        arguments.reserve(curr->operands.size());
        for (auto expression : curr->operands) {
          Flow flow = visit(expression);
          if (flow.breaking()) return flow;
          arguments.push_back(flow.value);
        }
        return callFunction(curr->target, arguments);
      }
      Flow visitCallImport(CallImport *curr) override {
      }
      Flow visitCallIndirect(CallIndirect *curr) override {
      }
      Flow visitGetLocal(GetLocal *curr) override {
      }
      Flow visitSetLocal(SetLocal *curr) override {
      }
      Flow visitLoad(Load *curr) override {
      }
      Flow visitStore(Store *curr) override {
      }
      Flow visitConst(Const *curr) override {
        return Flow(curr->value); // heh
      }
      Flow visitUnary(Unary *curr) override {
      }
      Flow visitBinary(Binary *curr) override {
      }
      Flow visitCompare(Compare *curr) override {
      }
      Flow visitConvert(Convert *curr) override {
      }
      Flow visitHost(Host *curr) override {
      }
      Flow visitNop(Nop *curr) override {
      }
    };

    Function *function = functions[name];
    FunctionScope scope(function, arguments);
    return ExpressionRunner(scope).visit(function->body).value;
  }

private:
  Module& wasm;

  std::map<IString, Function*> functions;

};

} // namespace wasm
