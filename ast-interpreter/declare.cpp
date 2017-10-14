//
// Created by diamo on 2017/10/12.
//

#include "Environment.h"

void Environment::declRef(DeclRefExpr *declRefExpr) {
    mStack.front().setPC(declRefExpr);
    QualType qualType = declRefExpr->getType();
    Decl *decl = declRefExpr->getFoundDecl();

    assert(decl);

    if (qualType->isPointerType() || qualType->isArrayType()) {
        Value val = getDeclVal(decl);
        if (const char *id = getDeclstr(decl)) {
            log(PointerVisit || ArrayVisit, "Referenced var: %s\n", id);
        } else {
            logs(PointerVisit || ArrayVisit, "Referenced var id not found\n");
        }
        logp(PointerVisit || ArrayVisit, val.address);
        mStack.front().bindStmt(declRefExpr, val);

    } else if (qualType->isIntegerType()) {
        Value val = getDeclVal(decl);
        log_var(DeclVisit, val.intValue);
        mStack.front().bindStmt(declRefExpr, val);

    } else if (declRefExpr->getType()->isFunctionType()) {

    } else {
        assert(false && "Unexpected decl ref");
    }
}

void Environment::decl(DeclStmt *declStmt) {
    logs(FunctionCall, "Visiting DeclStmt\n");
    for (DeclGroupRef::iterator it = declStmt->decl_begin(),
                 ie = declStmt->decl_end();
         it != ie; ++it) {
        Decl *decl = *it;
        if (VarDecl *var_decl = dyn_cast<VarDecl>(decl)) {
            varDeclVisit(var_decl);
        }
    }
}

void Environment::varDeclVisit(VarDecl *varDecl) {
    Expr *initExpr = varDecl->getInit();
    //<editor-fold desc="Integer init">
    if (initExpr) {
        if (IntegerLiteral *IL = dyn_cast<IntegerLiteral>(initExpr)) {
            // TODO: support negative number
            int val = static_cast<int>(IL->getValue().getLimitedValue());
            log_var(DeclVisit, val);
            mStack.front().bindDecl(varDecl, Value(val));
            return;
        }
    }
    //</editor-fold>

    // TODO: Let consumer know "This is default value, not initialized"

    QualType decl_type = varDecl->getType();
    int pLevel = getPointerLevel(decl_type);

    Value default_value;
    if (pLevel == 0) {
        logs(PointerVisit || ArrayVisit, "Not pointer\n");

        if (decl_type->isArrayType()) { // is array
            // only support int []

            // Get array size
            auto array = dyn_cast<ConstantArrayType>(decl_type);
            assert(array);
            uint64_t array_size = array->getSize().getLimitedValue();
            log_var(ArrayVisit, (int) array_size);

            auto member_type = array->getPointeeOrArrayElementType();
            auto member_type_info = context.getTypeInfo(member_type);

            //NOTE that array is placed in heap
            void *allocatedAddress =
                    heap.Malloc((int) (array_size * member_type_info.Width / 8));
            logp(ArrayVisit, allocatedAddress);
            mStack.front().bindDecl(varDecl, Value(allocatedAddress));

        } else {
            default_value.typ = Int;
            mStack.front().bindDecl(varDecl, default_value);
        }

    } else {
        log(PointerVisit, "Lv %i pointer\n", pLevel);

        if (initExpr) {
            logp(PointerVisit, (void *) initExpr);
            Value initVal = mStack.front().getStmtVal(initExpr);

            // pointer info can only be obtained from here

            logp(PointerVisit, initVal.address);
            mStack.front().bindDecl(varDecl, initVal);

        } else {
            default_value.typ = Address;
            mStack.front().bindDecl(varDecl, default_value);
        }
    }
}


Value Environment::getDeclVal(Decl *decl) {
    if (mStack.front().hasDecl(decl)) {
        return mStack.front().getDeclVal(decl);
    } else {
        assert(mStack.back().hasDecl(decl));
        return mStack.back().getDeclVal(decl);
    }
}

