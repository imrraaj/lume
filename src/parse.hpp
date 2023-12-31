#pragma once

#include "ast.hpp"
#include "parse.hpp"
#include "token.hpp"
#include <cassert>

class Parser {
public:
    explicit Parser(std::vector<Token>&& tokens)
        : m_tokens(std::move(tokens))
        , index(0)
    {
    }

    std::optional<ast::NodeTerm*> parse_term()
    {
        if (peak().has_value() && peak().value()->type == TokenType::sub) {
            auto* neg = m_alloc.alloc<ast::NodeTermNeg>();
            neg->tok_sign = consume();
            if (auto term_base = parse_term_base()) {
                neg->term_base = term_base.value();
            }
            else {
                error("Expected term");
            }
            auto* term = m_alloc.alloc<ast::NodeTerm>();
            term->var = neg;
            return term;
        }
        else if (auto term_base = parse_term_base()) {
            auto* term = m_alloc.alloc<ast::NodeTerm>();
            term->var = term_base.value();
            return term;
        }
        return {};
    }

    std::optional<ast::NodeTermBase*> parse_term_base()
    {
        if (!peak().has_value()) {
            return {};
        }
        auto* term_base = m_alloc.alloc<ast::NodeTermBase>();
        if (peak().value()->type == TokenType::left_paren) {
            auto* term_base_paren = m_alloc.alloc<ast::NodeTermBaseParen>();
            term_base_paren->tok_left_paren = consume();
            if (auto expr = parse_expr()) {
                term_base_paren->expr = expr.value();
            }
            else {
                error("Expected expression");
            }
            if (peak().has_value() && peak().value()->type == TokenType::right_paren) {
                term_base_paren->tok_right_paren = consume();
            }
            else {
                error("Expected `)`");
            }
            term_base->var = term_base_paren;
            return term_base;
        }
        else if (peak().value()->type == TokenType::int_lit) {
            auto* term_base_num = m_alloc.alloc<ast::NodeTermBaseNum>();
            term_base_num->tok_num = consume();
            term_base->var = term_base_num;
            return term_base;
        }
        else if (peak().value()->type == TokenType::ident) {
            auto* term_base_ident = m_alloc.alloc<ast::NodeTermBaseIdent>();
            term_base_ident->tok_ident = consume();
            term_base->var = term_base_ident;
            return term_base;
        }
        else if (peak().value()->type == TokenType::str_lit) {
            auto* term_base_str = m_alloc.alloc<ast::NodeTermBaseStr>();
            term_base_str->tok_str_lit = consume();
            term_base->var = term_base_str;
            return term_base;
        }
        else if (peak().value()->type == TokenType::true_) {
            auto* term_base_true = m_alloc.alloc<ast::NodeTermBaseTrue>();
            term_base_true->tok_true = consume();
            term_base->var = term_base_true;
            return term_base;
        }
        else if (peak().value()->type == TokenType::false_) {
            auto* term_base_false = m_alloc.alloc<ast::NodeTermBaseFalse>();
            term_base_false->tok_false = consume();
            term_base->var = term_base_false;
            return term_base;
        }
        else if (
            peak(2).has_value() && peak().value()->type == TokenType::inc
            && peak(2).value()->type == TokenType::ident) {
            auto* term_base_inc = m_alloc.alloc<ast::NodeTermBaseInc>();
            term_base_inc->tok_inc = consume();
            term_base_inc->tok_ident = consume();
            term_base->var = term_base_inc;
            return term_base;
        }
        else if (
            peak(2).has_value() && peak().value()->type == TokenType::dec
            && peak(2).value()->type == TokenType::ident) {
            auto* term_base_dec = m_alloc.alloc<ast::NodeTermBaseDec>();
            term_base_dec->tok_dec = consume();
            term_base_dec->tok_ident = consume();
            term_base->var = term_base_dec;
            return term_base;
        }
        else {
            return {};
        }
    }

    std::optional<ast::NodeExpr*> parse_expr()
    {
        auto term = parse_term();
        if (!term.has_value()) {
            return {};
        }
        auto* expr = m_alloc.alloc<ast::NodeExpr>();
        expr->var = term.value();
        int last_prec = 0;
        while (true) {
            if (!peak().has_value() || !is_bin_op(peak().value()->type)) {
                break;
            }
            const Token* last_op = peak().value();
            auto* expr_bin = m_alloc.alloc<ast::NodeExprBin>();
            if (bin_prec(peak().value()->type) <= last_prec) {
                expr_bin->lhs = expr;
                expr_bin->tok_op = consume();
                if (auto rhs_term = parse_term()) {
                    auto* rhs = m_alloc.alloc<ast::NodeExpr>();
                    rhs->var = rhs_term.value();
                    expr_bin->rhs = rhs;
                    auto* new_expr = m_alloc.alloc<ast::NodeExpr>();
                    new_expr->var = expr_bin;
                    expr = new_expr;
                }
                else {
                    error("Expected term");
                }
            }
            else {
                if (std::get_if<ast::NodeTerm*>(&expr->var)) {
                    expr_bin->lhs = expr;
                    expr_bin->tok_op = consume();
                    if (auto rhs_term = parse_term()) {
                        auto* rhs = m_alloc.alloc<ast::NodeExpr>();
                        rhs->var = rhs_term.value();
                        expr_bin->rhs = rhs;
                        auto* new_expr = m_alloc.alloc<ast::NodeExpr>();
                        new_expr->var = expr_bin;
                        expr = new_expr;
                    }
                    else {
                        error("Expected term");
                    }
                }
                else if (auto node_expr = std::get_if<ast::NodeExprBin*>(&expr->var)) {
                    expr_bin->lhs = (*node_expr)->rhs;
                    expr_bin->tok_op = consume();
                    if (auto rhs_term = parse_term()) {
                        auto* rhs = m_alloc.alloc<ast::NodeExpr>();
                        rhs->var = rhs_term.value();
                        expr_bin->rhs = rhs;
                        auto* new_expr = m_alloc.alloc<ast::NodeExpr>();
                        new_expr->var = expr_bin;
                        (*node_expr)->rhs = new_expr;
                    }
                    else {
                        error("Expected term");
                    }
                }
                else {
                    // Unreachable
                    assert(false);
                }
            }
            last_prec = bin_prec(last_op->type);
        }
        return expr;
    }

    std::optional<ast::NodeScope*> parse_scope()
    {
        if (peak().has_value() && peak().value()->type == TokenType::left_curly) {
            auto* scope = m_alloc.alloc<ast::NodeScope>();
            scope->tok_left_curly = consume();
            scope->block = parse_block();
            if (peak().has_value() && peak().value()->type == TokenType::right_curly) {
                scope->tok_right_curly = consume();
            }
            else {
                error("Expected `}`");
            }
            return scope;
        }
        return {};
    }

    std::optional<ast::NodeElse*> parse_else()
    {
        if (peak().has_value() && peak().value()->type == TokenType::else_) {
            auto* else_ = m_alloc.alloc<ast::NodeElse>();
            else_->tok_else = consume();
            if (auto scope = parse_scope()) {
                else_->scope = scope.value();
            }
            else {
                error("Expected scope with `{` and `}`");
            }
            return else_;
        }
        return {};
    }

    std::optional<ast::NodeStmt*> parse_stmt()
    {
        auto* stmt = m_alloc.alloc<ast::NodeStmt>();
        if (peak(2).has_value() && peak().value()->type == TokenType::ident && peak(2).value()->type == TokenType::eq) {
            auto* stmt_eq = m_alloc.alloc<ast::NodeStmtEq>();
            stmt_eq->tok_ident = consume();
            stmt_eq->tok_eq = consume();
            if (auto expr = parse_expr()) {
                stmt_eq->expr = expr.value();
            }
            else {
                error("Expected expression");
            }
            stmt->var = stmt_eq;
            return stmt;
        }
        else if (
            peak(2).has_value() && peak().value()->type == TokenType::print
            && peak(2).value()->type == TokenType::left_paren) {
            auto print_stmt = m_alloc.alloc<ast::NodeStmtPrint>();
            print_stmt->tok_print = consume();
            print_stmt->tok_left_paren = consume();
            if (auto expr = parse_expr()) {
                print_stmt->expr = expr.value();
            }
            else {
                error("Expected expression");
            }
            if (peak().has_value() && peak().value()->type == TokenType::right_paren) {
                print_stmt->tok_right_paren = consume();
            }
            else {
                error("Expected `)`");
            }
            stmt->var = print_stmt;
            return stmt;
        }
        else if (
            peak(3).has_value() && peak().value()->type == TokenType::var && peak(2).value()->type == TokenType::ident
            && peak(3).value()->type == TokenType::eq) {
            auto* stmt_var = m_alloc.alloc<ast::NodeStmtVar>();
            stmt_var->tok_var = consume();
            stmt_var->tok_ident = consume();
            stmt_var->tok_eq = consume();
            if (auto expr = parse_expr()) {
                stmt_var->expr = expr.value();
            }
            else {
                error("Expected expression");
            }
            stmt->var = stmt_var;
            return stmt;
        }
        else if (
            peak(2).has_value() && peak().value()->type == TokenType::write
            && peak(2).value()->type == TokenType::left_paren) {
            auto stmt_write = m_alloc.alloc<ast::NodeStmtWrite>();
            stmt_write->tok_write = consume();
            stmt_write->tok_left_paren = consume();
            if (auto expr1 = parse_expr()) {
                stmt_write->expr1 = expr1.value();
            }
            else {
                error("Expected expression");
            }
            if (peak().has_value() && peak().value()->type == TokenType::comma) {
                stmt_write->tok_comma = consume();
            }
            else {
                error("Expected `,`");
            }
            if (auto expr2 = parse_expr()) {
                stmt_write->expr2 = expr2.value();
            }
            else {
                error("Expected expression");
            }
            if (peak().has_value() && peak().value()->type == TokenType::right_paren) {
                stmt_write->tok_right_paren = consume();
            }
            else {
                error("Expected `)`");
            }
            stmt->var = stmt_write;
            return stmt;
        }
        else if (auto expr = parse_expr()) {
            auto stmt_expr = m_alloc.alloc<ast::NodeStmtExpr>();
            stmt_expr->expr = expr.value();
            stmt->var = stmt_expr;
            return stmt;
        }
        return {};
    }

    std::optional<ast::NodeControl*> parse_control()
    {
        if (peak(2).has_value() && peak().value()->type == TokenType::if_
            && peak(2).value()->type == TokenType::left_paren) {
            auto control = m_alloc.alloc<ast::NodeControl>();
            auto* control_if = m_alloc.alloc<ast::NodeControlIf>();
            control_if->tok_if = consume();
            control_if->tok_left_paren = consume();
            if (auto expr = parse_expr()) {
                control_if->expr = expr.value();
            }
            else {
                error("Expected expression");
            }
            if (peak().has_value() && peak().value()->type == TokenType::right_paren) {
                control_if->tok_right_paren = consume();
            }
            else {
                error("Expected `)`");
            }
            if (auto scope = parse_scope()) {
                control_if->scope = scope.value();
            }
            else {
                error("Expected scope with `{` and `}`");
            }
            control_if->else_ = parse_else();
            control->var = control_if;
            return control;
        }
        else if (auto scope = parse_scope()) {
            auto control = m_alloc.alloc<ast::NodeControl>();
            auto control_scope = m_alloc.alloc<ast::NodeControlScope>();
            control_scope->scope = scope.value();
            control->var = control_scope;
            return control;
        }
        else {
            return {};
        }
    }

    std::optional<ast::NodeBlock*> parse_block()
    {
        if (auto stmt = parse_stmt()) {
            auto block = m_alloc.alloc<ast::NodeBlock>();
            auto block_stmt = m_alloc.alloc<ast::NodeBlockStmt>();
            block_stmt->stmt = stmt.value();
            if (peak().has_value() && peak().value()->type == TokenType::semi) {
                block_stmt->tok_semi = consume();
            }
            else {
                error("Expected `;`");
            }
            block_stmt->next = parse_block();
            block->var = block_stmt;
            return block;
        }
        else if (auto control = parse_control()) {
            auto block = m_alloc.alloc<ast::NodeBlock>();
            auto block_control = m_alloc.alloc<ast::NodeBlockControl>();
            block_control->control = control.value();
            block_control->next = parse_block();
            block->var = block_control;
            return block;
        }
        else {
            return {};
        }
    }

private:
    std::optional<const Token*> peak(int ahead = 1)
    {
        if (index + (ahead - 1) < m_tokens.size()) {
            return &m_tokens.at(index + (ahead - 1));
        }
        else {
            return {};
        }
    }

    const Token* consume()
    {
        index++;
        return &m_tokens.at(index - 1);
    }

    static void error(const std::string& msg)
    {
        std::cerr << "[Error] " << msg << std::endl;
        exit(EXIT_FAILURE);
    }

    Alloc m_alloc {};

    const std::vector<Token> m_tokens;
    size_t index;
};