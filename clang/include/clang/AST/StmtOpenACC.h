//===- StmtOpenACC.h - Classes for OpenACC directives  ----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
/// \file
/// This file defines OpenACC AST classes for statement-level contructs.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_STMTOPENACC_H
#define LLVM_CLANG_AST_STMTOPENACC_H

#include "clang/AST/OpenACCClause.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/OpenACCKinds.h"
#include "clang/Basic/SourceLocation.h"
#include <memory>

namespace clang {
/// This is the base class for an OpenACC statement-level construct, other
/// construct types are expected to inherit from this.
class OpenACCConstructStmt : public Stmt {
  friend class ASTStmtWriter;
  friend class ASTStmtReader;
  /// The directive kind. Each implementation of this interface should handle
  /// specific kinds.
  OpenACCDirectiveKind Kind = OpenACCDirectiveKind::Invalid;
  /// The location of the directive statement, from the '#' to the last token of
  /// the directive.
  SourceRange Range;
  /// The location of the directive name.
  SourceLocation DirectiveLoc;

  /// The list of clauses.  This is stored here as an ArrayRef, as this is the
  /// most convienient place to access the list, however the list itself should
  /// be stored in leaf nodes, likely in trailing-storage.
  MutableArrayRef<const OpenACCClause *> Clauses;

protected:
  OpenACCConstructStmt(StmtClass SC, OpenACCDirectiveKind K,
                       SourceLocation Start, SourceLocation DirectiveLoc,
                       SourceLocation End)
      : Stmt(SC), Kind(K), Range(Start, End), DirectiveLoc(DirectiveLoc) {}

  // Used only for initialization, the leaf class can initialize this to
  // trailing storage.
  void setClauseList(MutableArrayRef<const OpenACCClause *> NewClauses) {
    assert(Clauses.empty() && "Cannot change clause list");
    Clauses = NewClauses;
  }

public:
  OpenACCDirectiveKind getDirectiveKind() const { return Kind; }

  static bool classof(const Stmt *S) {
    return S->getStmtClass() >= firstOpenACCConstructStmtConstant &&
           S->getStmtClass() <= lastOpenACCConstructStmtConstant;
  }

  SourceLocation getBeginLoc() const { return Range.getBegin(); }
  SourceLocation getEndLoc() const { return Range.getEnd(); }
  SourceLocation getDirectiveLoc() const { return DirectiveLoc; }
  ArrayRef<const OpenACCClause *> clauses() const { return Clauses; }

  child_range children() {
    return child_range(child_iterator(), child_iterator());
  }

  const_child_range children() const {
    return const_cast<OpenACCConstructStmt *>(this)->children();
  }
};

/// This is a base class for any OpenACC statement-level constructs that have an
/// associated statement. This class is not intended to be instantiated, but is
/// a convenient place to hold the associated statement.
class OpenACCAssociatedStmtConstruct : public OpenACCConstructStmt {
  friend class ASTStmtWriter;
  friend class ASTStmtReader;
  template <typename Derived> friend class RecursiveASTVisitor;
  Stmt *AssociatedStmt = nullptr;

protected:
  OpenACCAssociatedStmtConstruct(StmtClass SC, OpenACCDirectiveKind K,
                                 SourceLocation Start,
                                 SourceLocation DirectiveLoc,
                                 SourceLocation End, Stmt *AssocStmt)
      : OpenACCConstructStmt(SC, K, Start, DirectiveLoc, End),
        AssociatedStmt(AssocStmt) {}

  void setAssociatedStmt(Stmt *S) { AssociatedStmt = S; }
  Stmt *getAssociatedStmt() { return AssociatedStmt; }
  const Stmt *getAssociatedStmt() const {
    return const_cast<OpenACCAssociatedStmtConstruct *>(this)
        ->getAssociatedStmt();
  }

public:
  static bool classof(const Stmt *T) {
    return false;
  }

  child_range children() {
    if (getAssociatedStmt())
      return child_range(&AssociatedStmt, &AssociatedStmt + 1);
    return child_range(child_iterator(), child_iterator());
  }

  const_child_range children() const {
    return const_cast<OpenACCAssociatedStmtConstruct *>(this)->children();
  }
};

/// This class represents a compute construct, representing a 'Kind' of
/// `parallel', 'serial', or 'kernel'. These constructs are associated with a
/// 'structured block', defined as:
///
///  in C or C++, an executable statement, possibly compound, with a single
///  entry at the top and a single exit at the bottom
///
/// At the moment there is no real motivation to have a different AST node for
/// those three, as they are semantically identical, and have only minor
/// differences in the permitted list of clauses, which can be differentiated by
/// the 'Kind'.
class OpenACCComputeConstruct final
    : public OpenACCAssociatedStmtConstruct,
      public llvm::TrailingObjects<OpenACCComputeConstruct,
                                   const OpenACCClause *> {
  friend class ASTStmtWriter;
  friend class ASTStmtReader;
  friend class ASTContext;
  OpenACCComputeConstruct(unsigned NumClauses)
      : OpenACCAssociatedStmtConstruct(
            OpenACCComputeConstructClass, OpenACCDirectiveKind::Invalid,
            SourceLocation{}, SourceLocation{}, SourceLocation{},
            /*AssociatedStmt=*/nullptr) {
    // We cannot send the TrailingObjects storage to the base class (which holds
    // a reference to the data) until it is constructed, so we have to set it
    // separately here.
    std::uninitialized_value_construct(
        getTrailingObjects<const OpenACCClause *>(),
        getTrailingObjects<const OpenACCClause *>() + NumClauses);
    setClauseList(MutableArrayRef(getTrailingObjects<const OpenACCClause *>(),
                                  NumClauses));
  }

  OpenACCComputeConstruct(OpenACCDirectiveKind K, SourceLocation Start,
                          SourceLocation DirectiveLoc, SourceLocation End,
                          ArrayRef<const OpenACCClause *> Clauses,
                          Stmt *StructuredBlock)
      : OpenACCAssociatedStmtConstruct(OpenACCComputeConstructClass, K, Start,
                                       DirectiveLoc, End, StructuredBlock) {
    assert(isOpenACCComputeDirectiveKind(K) &&
           "Only parallel, serial, and kernels constructs should be "
           "represented by this type");

    // Initialize the trailing storage.
    std::uninitialized_copy(Clauses.begin(), Clauses.end(),
                            getTrailingObjects<const OpenACCClause *>());

    setClauseList(MutableArrayRef(getTrailingObjects<const OpenACCClause *>(),
                                  Clauses.size()));
  }

  void setStructuredBlock(Stmt *S) { setAssociatedStmt(S); }

public:
  static bool classof(const Stmt *T) {
    return T->getStmtClass() == OpenACCComputeConstructClass;
  }

  static OpenACCComputeConstruct *CreateEmpty(const ASTContext &C,
                                              unsigned NumClauses);
  static OpenACCComputeConstruct *
  Create(const ASTContext &C, OpenACCDirectiveKind K, SourceLocation BeginLoc,
         SourceLocation DirectiveLoc, SourceLocation EndLoc,
         ArrayRef<const OpenACCClause *> Clauses, Stmt *StructuredBlock);

  Stmt *getStructuredBlock() { return getAssociatedStmt(); }
  const Stmt *getStructuredBlock() const {
    return const_cast<OpenACCComputeConstruct *>(this)->getStructuredBlock();
  }
};
/// This class represents a 'loop' construct.  The 'loop' construct applies to a
/// 'for' loop (or range-for loop), and is optionally associated with a Compute
/// Construct.
class OpenACCLoopConstruct final
    : public OpenACCAssociatedStmtConstruct,
      public llvm::TrailingObjects<OpenACCLoopConstruct,
                                   const OpenACCClause *> {
  // The compute/combined construct kind this loop is associated with, or
  // invalid if this is an orphaned loop construct.
  OpenACCDirectiveKind ParentComputeConstructKind =
      OpenACCDirectiveKind::Invalid;

  friend class ASTStmtWriter;
  friend class ASTStmtReader;
  friend class ASTContext;
  friend class OpenACCAssociatedStmtConstruct;
  friend class OpenACCCombinedConstruct;
  friend class OpenACCComputeConstruct;

  OpenACCLoopConstruct(unsigned NumClauses);

  OpenACCLoopConstruct(OpenACCDirectiveKind ParentKind, SourceLocation Start,
                       SourceLocation DirLoc, SourceLocation End,
                       ArrayRef<const OpenACCClause *> Clauses, Stmt *Loop);

public:
  static bool classof(const Stmt *T) {
    return T->getStmtClass() == OpenACCLoopConstructClass;
  }

  static OpenACCLoopConstruct *CreateEmpty(const ASTContext &C,
                                           unsigned NumClauses);

  static OpenACCLoopConstruct *
  Create(const ASTContext &C, OpenACCDirectiveKind ParentKind,
         SourceLocation BeginLoc, SourceLocation DirLoc, SourceLocation EndLoc,
         ArrayRef<const OpenACCClause *> Clauses, Stmt *Loop);

  Stmt *getLoop() { return getAssociatedStmt(); }
  const Stmt *getLoop() const {
    return const_cast<OpenACCLoopConstruct *>(this)->getLoop();
  }

  /// OpenACC 3.3 2.9:
  /// An orphaned loop construct is a loop construct that is not lexically
  /// enclosed within a compute construct. The parent compute construct of a
  /// loop construct is the nearest compute construct that lexically contains
  /// the loop construct.
  bool isOrphanedLoopConstruct() const {
    return ParentComputeConstructKind == OpenACCDirectiveKind::Invalid;
  }

  OpenACCDirectiveKind getParentComputeConstructKind() const {
    return ParentComputeConstructKind;
  }
};

// This class represents a 'combined' construct, which has a bunch of rules
// shared with both loop and compute constructs.
class OpenACCCombinedConstruct final
    : public OpenACCAssociatedStmtConstruct,
      public llvm::TrailingObjects<OpenACCCombinedConstruct,
                                   const OpenACCClause *> {
  OpenACCCombinedConstruct(unsigned NumClauses)
      : OpenACCAssociatedStmtConstruct(
            OpenACCCombinedConstructClass, OpenACCDirectiveKind::Invalid,
            SourceLocation{}, SourceLocation{}, SourceLocation{},
            /*AssociatedStmt=*/nullptr) {
    std::uninitialized_value_construct(
        getTrailingObjects<const OpenACCClause *>(),
        getTrailingObjects<const OpenACCClause *>() + NumClauses);
    setClauseList(MutableArrayRef(getTrailingObjects<const OpenACCClause *>(),
                                  NumClauses));
  }

  OpenACCCombinedConstruct(OpenACCDirectiveKind K, SourceLocation Start,
                           SourceLocation DirectiveLoc, SourceLocation End,
                           ArrayRef<const OpenACCClause *> Clauses,
                           Stmt *StructuredBlock)
      : OpenACCAssociatedStmtConstruct(OpenACCCombinedConstructClass, K, Start,
                                       DirectiveLoc, End, StructuredBlock) {
    assert(isOpenACCCombinedDirectiveKind(K) &&
           "Only parallel loop, serial loop, and kernels loop constructs "
           "should be represented by this type");

    std::uninitialized_copy(Clauses.begin(), Clauses.end(),
                            getTrailingObjects<const OpenACCClause *>());
    setClauseList(MutableArrayRef(getTrailingObjects<const OpenACCClause *>(),
                                  Clauses.size()));
  }
  void setStructuredBlock(Stmt *S) { setAssociatedStmt(S); }

public:
  static bool classof(const Stmt *T) {
    return T->getStmtClass() == OpenACCCombinedConstructClass;
  }

  static OpenACCCombinedConstruct *CreateEmpty(const ASTContext &C,
                                               unsigned NumClauses);
  static OpenACCCombinedConstruct *
  Create(const ASTContext &C, OpenACCDirectiveKind K, SourceLocation Start,
         SourceLocation DirectiveLoc, SourceLocation End,
         ArrayRef<const OpenACCClause *> Clauses, Stmt *StructuredBlock);
  Stmt *getLoop() { return getAssociatedStmt(); }
  const Stmt *getLoop() const {
    return const_cast<OpenACCCombinedConstruct *>(this)->getLoop();
  }
};

// This class represents a 'data' construct, which has an associated statement
// and clauses, but is otherwise pretty simple.
class OpenACCDataConstruct final
    : public OpenACCAssociatedStmtConstruct,
      public llvm::TrailingObjects<OpenACCDataConstruct,
                                   const OpenACCClause *> {
  OpenACCDataConstruct(unsigned NumClauses)
      : OpenACCAssociatedStmtConstruct(
            OpenACCDataConstructClass, OpenACCDirectiveKind::Data,
            SourceLocation{}, SourceLocation{}, SourceLocation{},
            /*AssociatedStmt=*/nullptr) {
    std::uninitialized_value_construct(
        getTrailingObjects<const OpenACCClause *>(),
        getTrailingObjects<const OpenACCClause *>() + NumClauses);
    setClauseList(MutableArrayRef(getTrailingObjects<const OpenACCClause *>(),
                                  NumClauses));
  }

  OpenACCDataConstruct(SourceLocation Start, SourceLocation DirectiveLoc,
                       SourceLocation End,
                       ArrayRef<const OpenACCClause *> Clauses,
                       Stmt *StructuredBlock)
      : OpenACCAssociatedStmtConstruct(OpenACCDataConstructClass,
                                       OpenACCDirectiveKind::Data, Start,
                                       DirectiveLoc, End, StructuredBlock) {
    std::uninitialized_copy(Clauses.begin(), Clauses.end(),
                            getTrailingObjects<const OpenACCClause *>());
    setClauseList(MutableArrayRef(getTrailingObjects<const OpenACCClause *>(),
                                  Clauses.size()));
  }
  void setStructuredBlock(Stmt *S) { setAssociatedStmt(S); }

public:
  static bool classof(const Stmt *T) {
    return T->getStmtClass() == OpenACCDataConstructClass;
  }

  static OpenACCDataConstruct *CreateEmpty(const ASTContext &C,
                                           unsigned NumClauses);
  static OpenACCDataConstruct *Create(const ASTContext &C, SourceLocation Start,
                                      SourceLocation DirectiveLoc,
                                      SourceLocation End,
                                      ArrayRef<const OpenACCClause *> Clauses,
                                      Stmt *StructuredBlock);
  Stmt *getStructuredBlock() { return getAssociatedStmt(); }
  const Stmt *getStructuredBlock() const {
    return const_cast<OpenACCDataConstruct *>(this)->getStructuredBlock();
  }
};
// This class represents a 'enter data' construct, which JUST has clauses.
class OpenACCEnterDataConstruct final
    : public OpenACCConstructStmt,
      public llvm::TrailingObjects<OpenACCEnterDataConstruct,
                                   const OpenACCClause *> {
  OpenACCEnterDataConstruct(unsigned NumClauses)
      : OpenACCConstructStmt(OpenACCEnterDataConstructClass,
                             OpenACCDirectiveKind::EnterData, SourceLocation{},
                             SourceLocation{}, SourceLocation{}) {
    std::uninitialized_value_construct(
        getTrailingObjects<const OpenACCClause *>(),
        getTrailingObjects<const OpenACCClause *>() + NumClauses);
    setClauseList(MutableArrayRef(getTrailingObjects<const OpenACCClause *>(),
                                  NumClauses));
  }
  OpenACCEnterDataConstruct(SourceLocation Start, SourceLocation DirectiveLoc,
                            SourceLocation End,
                            ArrayRef<const OpenACCClause *> Clauses)
      : OpenACCConstructStmt(OpenACCEnterDataConstructClass,
                             OpenACCDirectiveKind::EnterData, Start,
                             DirectiveLoc, End) {
    std::uninitialized_copy(Clauses.begin(), Clauses.end(),
                            getTrailingObjects<const OpenACCClause *>());
    setClauseList(MutableArrayRef(getTrailingObjects<const OpenACCClause *>(),
                                  Clauses.size()));
  }

public:
  static bool classof(const Stmt *T) {
    return T->getStmtClass() == OpenACCEnterDataConstructClass;
  }
  static OpenACCEnterDataConstruct *CreateEmpty(const ASTContext &C,
                                                unsigned NumClauses);
  static OpenACCEnterDataConstruct *
  Create(const ASTContext &C, SourceLocation Start, SourceLocation DirectiveLoc,
         SourceLocation End, ArrayRef<const OpenACCClause *> Clauses);
};
// This class represents a 'exit data' construct, which JUST has clauses.
class OpenACCExitDataConstruct final
    : public OpenACCConstructStmt,
      public llvm::TrailingObjects<OpenACCExitDataConstruct,
                                   const OpenACCClause *> {
  OpenACCExitDataConstruct(unsigned NumClauses)
      : OpenACCConstructStmt(OpenACCExitDataConstructClass,
                             OpenACCDirectiveKind::ExitData, SourceLocation{},
                             SourceLocation{}, SourceLocation{}) {
    std::uninitialized_value_construct(
        getTrailingObjects<const OpenACCClause *>(),
        getTrailingObjects<const OpenACCClause *>() + NumClauses);
    setClauseList(MutableArrayRef(getTrailingObjects<const OpenACCClause *>(),
                                  NumClauses));
  }
  OpenACCExitDataConstruct(SourceLocation Start, SourceLocation DirectiveLoc,
                           SourceLocation End,
                           ArrayRef<const OpenACCClause *> Clauses)
      : OpenACCConstructStmt(OpenACCExitDataConstructClass,
                             OpenACCDirectiveKind::ExitData, Start,
                             DirectiveLoc, End) {
    std::uninitialized_copy(Clauses.begin(), Clauses.end(),
                            getTrailingObjects<const OpenACCClause *>());
    setClauseList(MutableArrayRef(getTrailingObjects<const OpenACCClause *>(),
                                  Clauses.size()));
  }

public:
  static bool classof(const Stmt *T) {
    return T->getStmtClass() == OpenACCExitDataConstructClass;
  }
  static OpenACCExitDataConstruct *CreateEmpty(const ASTContext &C,
                                               unsigned NumClauses);
  static OpenACCExitDataConstruct *
  Create(const ASTContext &C, SourceLocation Start, SourceLocation DirectiveLoc,
         SourceLocation End, ArrayRef<const OpenACCClause *> Clauses);
};
// This class represents a 'host_data' construct, which has an associated
// statement and clauses, but is otherwise pretty simple.
class OpenACCHostDataConstruct final
    : public OpenACCAssociatedStmtConstruct,
      public llvm::TrailingObjects<OpenACCHostDataConstruct,
                                   const OpenACCClause *> {
  OpenACCHostDataConstruct(unsigned NumClauses)
      : OpenACCAssociatedStmtConstruct(
            OpenACCHostDataConstructClass, OpenACCDirectiveKind::HostData,
            SourceLocation{}, SourceLocation{}, SourceLocation{},
            /*AssociatedStmt=*/nullptr) {
    std::uninitialized_value_construct(
        getTrailingObjects<const OpenACCClause *>(),
        getTrailingObjects<const OpenACCClause *>() + NumClauses);
    setClauseList(MutableArrayRef(getTrailingObjects<const OpenACCClause *>(),
                                  NumClauses));
  }
  OpenACCHostDataConstruct(SourceLocation Start, SourceLocation DirectiveLoc,
                           SourceLocation End,
                           ArrayRef<const OpenACCClause *> Clauses,
                           Stmt *StructuredBlock)
      : OpenACCAssociatedStmtConstruct(OpenACCHostDataConstructClass,
                                       OpenACCDirectiveKind::HostData, Start,
                                       DirectiveLoc, End, StructuredBlock) {
    std::uninitialized_copy(Clauses.begin(), Clauses.end(),
                            getTrailingObjects<const OpenACCClause *>());
    setClauseList(MutableArrayRef(getTrailingObjects<const OpenACCClause *>(),
                                  Clauses.size()));
  }
  void setStructuredBlock(Stmt *S) { setAssociatedStmt(S); }

public:
  static bool classof(const Stmt *T) {
    return T->getStmtClass() == OpenACCHostDataConstructClass;
  }
  static OpenACCHostDataConstruct *CreateEmpty(const ASTContext &C,
                                               unsigned NumClauses);
  static OpenACCHostDataConstruct *
  Create(const ASTContext &C, SourceLocation Start, SourceLocation DirectiveLoc,
         SourceLocation End, ArrayRef<const OpenACCClause *> Clauses,
         Stmt *StructuredBlock);
  Stmt *getStructuredBlock() { return getAssociatedStmt(); }
  const Stmt *getStructuredBlock() const {
    return const_cast<OpenACCHostDataConstruct *>(this)->getStructuredBlock();
  }
};
} // namespace clang
#endif // LLVM_CLANG_AST_STMTOPENACC_H
