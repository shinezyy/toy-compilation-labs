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
        mEnv->binop(bop);
    }

    virtual void VisitDeclRefExpr(DeclRefExpr *expr) {
        VisitStmt(expr);
        mEnv->declref(expr);
    }

    virtual void VisitCastExpr(CastExpr *expr) {
        VisitStmt(expr);
        mEnv->cast(expr);
    }

    virtual void VisitCallExpr(CallExpr *call) {
        VisitStmt(call);
        if (FunctionDecl *functionDecl = mEnv->call(call)) {
            VisitStmt(functionDecl->getBody());
        }
    }

    virtual void VisitDeclStmt(DeclStmt *declStmt) {
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
            this->Visit(ifStmt->getThen());
        } else if (Stmt *elseStmt = ifStmt->getThen()){
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

private:
    Environment *mEnv;
};

class InterpreterConsumer : public ASTConsumer {
public:
    explicit InterpreterConsumer(const ASTContext &context) : mEnv(),
                                                              mVisitor(context, &mEnv) {
    }

    virtual ~InterpreterConsumer() {}

    virtual void HandleTranslationUnit(clang::ASTContext &Context) {
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
