#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Mangle.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"
#include "clang/AST/TextNodeDumper.h"

using namespace std;
using namespace clang;
using namespace llvm;

namespace {
    class CaptorVisitor : public RecursiveASTVisitor<CaptorVisitor> {
    private:
        ASTContext &context;
        SourceManager &sourceManager;
        int inputLine;

    public:
        std::string outputFuncText;

        explicit CaptorVisitor(CompilerInstance &instance, int _inputLine) :
                context(instance.getASTContext()),
                sourceManager(instance.getSourceManager()), inputLine(_inputLine) {}

        std::string dumpOriginalCode(const SourceRange& sourceRange) {
            SourceLocation sLoc = sourceRange.getBegin();
            SourceLocation eLoc = Lexer::getLocForEndOfToken(sourceRange.getEnd(), 0, sourceManager, context.getLangOpts());
            const char* sPos = sourceManager.getCharacterData(sLoc);
            const char* ePos = sourceManager.getCharacterData(eLoc);
            if (sPos == nullptr || ePos == nullptr || ePos - sPos <= 0) {
                return "";
            }
            return std::move(std::string(sPos, ePos));
        }

        bool TraverseDecl(Decl *decl) {
            if (!decl) {
                return true;
            }

            auto *funcDecl = llvm::dyn_cast<FunctionDecl>(decl);
            if (funcDecl == nullptr || funcDecl->isImplicit() || funcDecl->getNameAsString().empty() ||
                !funcDecl->doesThisDeclarationHaveABody()) {
                return RecursiveASTVisitor::TraverseDecl(decl);
            }

            const auto &sr = funcDecl->getBody()->getSourceRange();
            FullSourceLoc sLoc(sr.getBegin(), sourceManager);
            if (sLoc.isInvalid()) {
                return RecursiveASTVisitor::TraverseDecl(decl);
            }
            unsigned int sLine = sLoc.getExpansionLineNumber();

            FullSourceLoc eLoc(Lexer::getLocForEndOfToken(sr.getEnd(), 0, sourceManager, context.getLangOpts()),
                               sourceManager);
            if (eLoc.isInvalid()) {
                return RecursiveASTVisitor::TraverseDecl(decl);
            }
            unsigned int eLine = eLoc.getExpansionLineNumber();

            if (inputLine <= sLine || eLine <= inputLine) {
                return RecursiveASTVisitor::TraverseDecl(decl);
            }

            outputFuncText = dumpOriginalCode(funcDecl->getSourceRange());

            return RecursiveASTVisitor::TraverseDecl(decl);
        }
    };

    class CaptorConsumer : public ASTConsumer {
    private:
        CompilerInstance &instance;
        int inputLine;
        std::string outputPath;

    public:
        explicit CaptorConsumer(CompilerInstance &_instance, int _inputLine, std::string _outputPath)
                : instance(_instance), inputLine(_inputLine), outputPath(std::move(_outputPath)) {}

        void HandleTranslationUnit(ASTContext &context) override {
            CaptorVisitor visitor(instance, inputLine);
            visitor.TraverseDecl(context.getTranslationUnitDecl());

            llvm::errs() << "outputFuncText:\n";
            llvm::errs() << visitor.outputFuncText;

            std::ofstream ofs(outputPath);
            if (!ofs.is_open()) {
                llvm::errs() << "Can not open " << outputPath << "\n";
                exit(1);
            }
            ofs << visitor.outputFuncText;
            ofs.close();
        }
    };

    class CaptorAction : public PluginASTAction {
    protected:
        int inputLine = 0;
        std::string outputPath;

        std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &instance,
                                                       llvm::StringRef) override {
            return std::make_unique<CaptorConsumer>(instance, inputLine, outputPath);
        }

        bool ParseArgs(const CompilerInstance &CI,
                       const std::vector<std::string> &args) override {
            if (args.size() != 2) {
                llvm::errs() << "Useage: <inputLine> <outputPath>\n";
                return false;
            }
            inputLine = std::stoi(args[0]);
            llvm::errs() << "inputLine:" << inputLine << "\n";

            outputPath = args[1];
            llvm::errs() << outputPath << "\n";

            return true;
        }

        // Automatically run the plugin after the main AST action
        PluginASTAction::ActionType getActionType() override {
            return AddAfterMainAction;
        }
    };

}

static FrontendPluginRegistry::Add<CaptorAction>
        X("captor", "benchmark plugin");
