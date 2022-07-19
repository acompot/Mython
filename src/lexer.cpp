#include "lexer.h"

#include <algorithm>
#include <charconv>
#include <unordered_map>
#include <vector>
#include <cctype>

using namespace std;

namespace parse {

bool operator==(const Token& lhs, const Token& rhs) {
    using namespace token_type;

    if (lhs.index() != rhs.index()) {
        return false;
    }
    if (lhs.Is<Char>()) {
        return lhs.As<Char>().value == rhs.As<Char>().value;
    }
    if (lhs.Is<Number>()) {
        return lhs.As<Number>().value == rhs.As<Number>().value;
    }
    if (lhs.Is<String>()) {
        return lhs.As<String>().value == rhs.As<String>().value;
    }
    if (lhs.Is<Id>()) {
        return lhs.As<Id>().value == rhs.As<Id>().value;
    }
    return true;
}

bool operator!=(const Token& lhs, const Token& rhs) {
    return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& os, const Token& rhs) {
    using namespace token_type;

#define VALUED_OUTPUT(type) \
    if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

    VALUED_OUTPUT(Number);
    VALUED_OUTPUT(Id);
    VALUED_OUTPUT(String);
    VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
    if (rhs.Is<type>()) return os << #type;

    UNVALUED_OUTPUT(Class);
    UNVALUED_OUTPUT(Return);
    UNVALUED_OUTPUT(If);
    UNVALUED_OUTPUT(Else);
    UNVALUED_OUTPUT(Def);
    UNVALUED_OUTPUT(Newline);
    UNVALUED_OUTPUT(Print);
    UNVALUED_OUTPUT(Indent);
    UNVALUED_OUTPUT(Dedent);
    UNVALUED_OUTPUT(And);
    UNVALUED_OUTPUT(Or);
    UNVALUED_OUTPUT(Not);
    UNVALUED_OUTPUT(Eq);
    UNVALUED_OUTPUT(NotEq);
    UNVALUED_OUTPUT(LessOrEq);
    UNVALUED_OUTPUT(GreaterOrEq);
    UNVALUED_OUTPUT(None);
    UNVALUED_OUTPUT(True);
    UNVALUED_OUTPUT(False);
    UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

    return os << "Unknown token :("sv;
}

static const vector<char> special_symbols = { '.', ',', '=', '(', ')', '<', '>', '-', '+', ':', '/', '*' };
static const vector<string> bool_symbols = { "=="s, ">="s, "<="s, "!="s };
static const vector<char> num_symbols = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };

Lexer::Lexer(std::istream& input)
    : input_(input)
    , current_token_(token_type::Newline{}) {
    current_token_ = LoadToken();
}

const Token& Lexer::CurrentToken() const {
    return current_token_;
}

Token Lexer::NextToken() {
    current_token_ = LoadToken();
    return current_token_;
}

string GetLine(std::istream& input, char delim = '\n') {
    string s;
    getline(input, s, delim);
    return s;
}

string ParseString(std::istream& input, char delim = '\n') {
    string s;
    while (input) {
        char c = input.get();
        if (c == '\\') {
            c = input.get();
            switch (c)
            {
            case 't':
                s.push_back('\t');
                break;
            case '\\':
                s.push_back('\\');
                break;
            case 'n':
                s.push_back('\n');
                break;
            default:
                s.push_back(c);
                break;
            }
        }
        else if (c == delim) {
            return s;
        }
        else {
            s.push_back(c);
        }        
    }
    return s;
}

Token Lexer::LoadToken() {
    char c = input_.peek();
    if (!input_ || c == -1) { 
        if (dent_ > 0) {
            --dent_;
            dent_buf_ = 0;
            return { token_type::Dedent{} };
        }
        if ( (current_token_.Is<token_type::Newline>()) || (current_token_.Is<token_type::Eof>()) 
            || (current_token_.Is<token_type::Dedent>()) ) {
            return  { token_type::Eof{} };
        }
        else return  { token_type::Newline{} };
    }
    if (c == '\n') {
        if (current_token_.Is<token_type::Newline>()) {
            do {
                input_.get();
            } while ((c = input_.peek()) == '\n');
        }
        else {
            input_.get();
            return { token_type::Newline{} };
        }
    }
    if (current_token_.Is<token_type::Newline>()) {
        if (c == ' ') {
            int space_counter = 0;
            do {
                input_.get();
                ++space_counter;
            } while ((c = input_.peek()) == ' ');
            if (c == '#' || c == '\n') {
                GetLine(input_);
                return LoadToken();
            }
            if (space_counter / 2 == dent_ + 1) {
                ++dent_;
                return { token_type::Indent{} };
            }
            if (space_counter / 2 < dent_) {
                --dent_;
                dent_buf_ = space_counter / 2;
                return { token_type::Dedent{} };
            }
        }
        else if (dent_ > 0) {
            --dent_;
            dent_buf_ = 0;
            return { token_type::Dedent{} };
        }
    }
    if (current_token_.Is<token_type::Dedent>()) {
        if (dent_buf_ < dent_) {
            --dent_;
            return { token_type::Dedent{} };
        }
    }
    if (c == ' ') {
        do {
            input_.get();
        } while ((c = input_.peek()) == ' ');
    }
    if (ispunct(c) && c != '_') {
        c = input_.get();
        switch (c) {        
        case '"': {           
            return { token_type::String{ ParseString(input_, '"') } }; }
        case '\'': {
            return { token_type::String{ ParseString(input_, '\'') } }; }
        case '#':
            GetLine(input_);
            if(!current_token_.Is<token_type::Newline>()) return { token_type::Newline{} };
            return LoadToken();
        case '=':
            if (input_.peek() != '=') {
                return { token_type::Char{'='} };
            }
            else {
                input_.get();
                return { token_type::Eq{} };
            }
        case '!':
            if (input_.peek() == '=') {
                input_.get();
                return { token_type::NotEq{} };
            }
            return { token_type::Char{'!'} };
        case '<':
            if (input_.peek() == '=') {
                input_.get();
                return { token_type::LessOrEq{} };
            }
            return { token_type::Char{'<'} };
        case '>':
            if (input_.peek() == '=') {
                input_.get();
                return { token_type::GreaterOrEq{} };
            }
            return { token_type::Char{'>'} };
        default:
            return { token_type::Char{c} };
        }
    }

    if (isalpha(c) || c == '_') {
        string s;
        while (isalnum(c) || c == '_') {
            s.push_back(static_cast<char>(input_.get()));
            c = input_.peek();
        }
        if (s == "class"s) {
            return { token_type::Class{} };
        }
        if (s == "def"s) {
            return { token_type::Def{} };
        }
        if (s == "print"s) {
            return { token_type::Print{} };
        }
        if (s == "return"s) {
            return { token_type::Return{} };
        }
        if (s == "if"s) {
            return { token_type::If{} };
        }
        if (s == "else"s) {
            return { token_type::Else{} };
        }
        if (s == "or"s) {
            return { token_type::Or{} };
        }
        if (s == "and"s) {
            return { token_type::And{} };
        }
        if (s == "not"s) {
            return { token_type::Not{} };
        }
        if (s == "None"s) {
            return { token_type::None{} };
        }
        if (s == "True"s) {
            return { token_type::True{} };
        }
        if (s == "False"s) {
            return { token_type::False{} };
        }
        return { token_type::Id{ s } };
    }
    if (isdigit(c)) {
        int i = input_.get() - 48;
        while (isdigit(input_.peek())) {
            i = i * 10 + (input_.get() - 48);
        }
        return { token_type::Number{ i } };
    }
    return { token_type::Eof{} };
}

}  // namespace parse
