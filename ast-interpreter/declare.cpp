//
// Created by diamo on 2017/10/12.
//

#include "Environment.h"

void Environment::declRef(DeclRefExpr *declRefexpr) {
    mStack.front().setPC(declRefexpr);
    QualType qualType = declRefexpr->getType();
    Decl *decl = declRefexpr->getFoundDecl();

    if (qualType->isPointerType() || qualType->isArrayType()) {
        assert(decl);
        Value val = mStack.front().getDeclVal(decl);
        if (const char *id = getDeclstr(decl)) {
            log(PointerVisit || ArrayVisit, "Referenced var: %s\n", id);
        } else {
            logs(PointerVisit || ArrayVisit, "Referenced var id not found\n");
        }
        logp(PointerVisit || ArrayVisit, val.address);
        mStack.front().bindStmt(declRefexpr, val);

    } else if (declRefexpr->getType()->isIntegerType()) {
        if (decl) {
            Value val = mStack.front().getDeclVal(decl);
            log_var(DeclVisit, val.intValue);
            mStack.front().bindStmt(declRefexpr, val);
        }

    } else if (declRefexpr->getType()->isFunctionType()) {

    } else {
        assert(false && "Unexpected decl ref");
    }
}

void Environment::decl(DeclStmt *declstmt) {
    logs(FunctionCall, "Visiting DeclStmt\n");
    for (DeclGroupRef::iterator it = declstmt->decl_begin(),
                 ie = declstmt->decl_end();
         it != ie; ++it) {
        Decl *decl = *it;
        if (VarDecl *var_decl = dyn_cast<VarDecl>(decl)) {
            Expr *initExpr = var_decl->getInit();
            //<editor-fold desc="Integer init">
            if (initExpr) {
                if (IntegerLiteral * IL = dyn_cast<IntegerLiteral>(initExpr)) {
                    // TODO: support negative number
                    int val = static_cast<int>(IL->getValue().getLimitedValue());
                    log_var(DeclVisit, val);
                    mStack.front().bindDecl(var_decl, Value(val));
                    continue;
                }
            }
            //</editor-fold>

            // TODO: Let consumer know "This is default value, not initialized"

            QualType decl_type = var_decl->getType();
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

                    auto member_type = array->getPointeeOrArrayElementType();
                    auto member_type_info = context.getTypeInfo(member_type);

                    //NOTE that array is placed in heap
                    void *allocatedAddress =
                            heap.Malloc((int)(array_size * member_type_info.Width / 8));
                    logp(ArrayVisit, allocatedAddress);
                    mStack.front().bindDecl(var_decl, Value(allocatedAddress));

                } else {
                    default_value.typ = Int;
                    mStack.front().bindDecl(var_decl, default_value);
                }

            } else {
                log(PointerVisit, "Lv %i pointer\n", pLevel);

                if (initExpr) {
                    logp(PointerVisit, (void *) initExpr);
                    Value initVal = mStack.front().getStmtVal(initExpr);

                    // pointer info can only be obtained from here

                    logp(PointerVisit, initVal.address);
                    mStack.front().bindDecl(var_decl, initVal);

                } else {
                    default_value.typ = Address;
                    mStack.front().bindDecl(var_decl, default_value);
                }
            }
        }
    }
}