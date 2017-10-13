//==--- tools/clang-check/ClangInterpreter.cpp - Clang Interpreter tool --------------===//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

#include "Environment.h"

class InterpreterVisitor :
        public EvaluatedExprVisitor<InterpreterVisitor> {
public:
    explicit InterpreterVisitor(const ASTContext &context, Environment *env)
            : EvaluatedExprVisitor(context), mEnv(env) {}

    virtual ~InterpreterVisitor() = default;

    virtual void VisitBinaryOperator(BinaryOperator *bop) {
        VisitStmt(bop);
        mEnv->binOp(bop);
    }

    virtual void VisitUnaryOperator(UnaryOperator *uop) {
        VisitStmt(uop);
        mEnv->unaryOp(uop);
    }

    virtual void VisitDeclRefExpr(DeclRefExpr *expr) {
        VisitStmt(expr);
        mEnv->declRef(expr);
    }

    virtual void VisitCastExpr(CastExpr *expr) {
        VisitStmt(expr);
        mEnv->cast(expr);
    }

    virtual void VisitImplicitCastExpr(ImplicitCastExpr *expr) {
        VisitStmt(expr);
        mEnv->implicitCast(expr);
    }

    virtual void VisitCallExpr(CallExpr *call) {
        VisitStmt(call);
        if (FunctionDecl *functionDecl = mEnv->call(call)) {
            VisitStmt(functionDecl->getBody());
            Value returnVal = mEnv->mStack.front().returnValue;
            mEnv->mStack.pop_front();
            mEnv->postCall(call, returnVal);
        }
    }

    virtual void VisitDeclStmt(DeclStmt *declStmt) {
        VisitStmt(declStmt);
        mEnv->decl(declStmt);
    }

    virtual void VisitParmVarDecl(ParmVarDecl *parmVarDecl) {
        mEnv->parmDecl(parmVarDecl);
    }

    virtual void VisitIfStmt(IfStmt *ifStmt) {
        Expr *condition_expr = ifStmt->getCond();
        this->Visit(condition_expr);
        logs(ControlStmt, "Got the condition_expr result.\n");
        if (mEnv->getCondition(condition_expr)) {
            logs(ControlStmt, "Cond is true, will visit then stmt.\n");
            this->Visit(ifStmt->getThen());
        } else if (Stmt *elseStmt = ifStmt->getElse()){
            logs(ControlStmt, "Cond is false, will visit else stmt.\n");
            this->Visit(elseStmt);
        }
    }

    virtual void VisitWhileStmt(WhileStmt *whileStmt) {
        Expr *condition_expr = whileStmt->getCond();
        this->Visit(condition_expr);
        while (mEnv->getCondition(condition_expr)) {
            this->Visit(whileStmt->getBody());
            this->Visit(condition_expr);
        }
    }

    virtual void VisitReturnStmt(ReturnStmt *returnStmt) {
        VisitStmt(returnStmt);
        mEnv->setReturnVal(returnStmt);
    }

    virtual void VisitUnaryExprOrTypeTraitExpr(UnaryExprOrTypeTraitExpr *expr) {
        VisitStmt(expr);
        mEnv->unaryExprOrTypeTraitExpr(expr);
    }

private:
    Environment *mEnv;
};

class InterpreterConsumer : public ASTConsumer {
public:
    explicit InterpreterConsumer(const ASTContext &context) : mEnv(context),
                                                              mVisitor(context, &mEnv) {
    }

    ~InterpreterConsumer() override = default;

    void HandleTranslationUnit(clang::ASTContext &Context) override {
        TranslationUnitDecl *decl = Context.getTranslationUnitDecl();
        mEnv.init(decl);

        FunctionDecl *entry = mEnv.getEntry();
        mVisitor.VisitStmt(entry->getBody());
    }

private:
    Environment mEnv;
    InterpreterVisitor mVisitor;
};

class InterpreterClassAction : public ASTFrontendAction {
public:
    virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
            clang::CompilerInstance &Compiler, llvm::StringRef InFile) {
        return std::unique_ptr<clang::ASTConsumer>(
                new InterpreterConsumer(Compiler.getASTContext()));
    }
};

int main(int argc, char **argv) {
    if (argc > 1) {
        clang::tooling::runToolOnCode(new InterpreterClassAction, argv[1]);
    }
}
