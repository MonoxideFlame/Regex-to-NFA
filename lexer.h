

typedef enum {
    EXPR_END = 0, EXPR_ERROR = 1, LPAREN = 2, RPAREN = 3,

    OP_BAR = 256, OP_CAT = 257, OP_STAR = 258, OP_PLUS = 259, OP_OPT = 260,

    OP_OR = 512, OP_AND = 513, OP_NOT = 514,

    SET_NUM = 768, SET_LOWER = 769, SET_UPPER = 770, SET_WORD = 771, SET_ALL= 772, SET_NONE = 773


    //ascii characters are just 1024 + their old values.
} TOKEN;

typedef struct {
    STRING8 source;
    U32 index;
    U16 previous_token;
    U16 current_token;
    B8 previous_mode;
    B8 current_mode;
    B8 is_charset;
   
   //converting to postfix 
    U64 stack_top;
    U16 shunting_yard_stack[512];

} REGEX_SCANNER;




char lexer_next_char(REGEX_SCANNER *lexer){
    if(lexer->index >= lexer->source.length){
        return 0;
    }
    char current = lexer->source.buffer[lexer->index];
    lexer->index++;
    return current;
}

char lexer_peek_char(REGEX_SCANNER* lexer){
    if(lexer->index >= lexer->source.length){
        return 0;
    }
    return lexer->source.buffer[lexer->index];
}

U16 lexer_parse_escape(char current){
    switch(current){
        case '\0':
            return EXPR_END;
        case 'n':
            return '\n' + 1024;
        break;
        case 'r':
            return '\r' + 1024;
        break;
        case 't':
            return '\t' + 1024;
        break;
        case 'v':
            return '\v' + 1024;
        break;
        case 'f':
            return '\f' + 1024;
        break;
        case 's':
            return ' ' + 1024;
        break;
        default:
            return current + 1024;
        break;
    }
}




U16 lexer_consume_regex(REGEX_SCANNER* lexer, char current){
    //special characters that have actual meaning:
    //
    //               ()[*+?|\
    //
    //
    switch (current) {
        case ')':
            return RPAREN;
            break;
        case '(':
            return LPAREN;
            break;
        case '[':
            lexer->is_charset = true;
            return LPAREN;
            break;
        case '*':
            return OP_STAR;
            break;
        case '+':
            return OP_PLUS;
            break;
        case '?':
            return OP_OPT;
            break;
        case '|':
            return OP_BAR;
            break;
        case '\\':
            current = lexer_next_char(lexer);
            return lexer_parse_escape(current);
            break;
        default:
            return 1024 + current;
    }

}


U16 lexer_predef_set(unsigned char current){
    //to upper
    if(((U8)(current - 97)) < 26){
        current -= 32;
    }
    switch(current){
        case '\0':
            return EXPR_END;
        case 'D':
            return SET_NUM; 
            break;
        case 'L':
            return SET_LOWER; 
            break;
        case 'U':
            return SET_UPPER; 
            break;
        case 'W':
            return SET_WORD; 
            break;
        case '.':
            return SET_ALL;
            break;
        case '0':
            return SET_NONE; 
            break;
        default:
            return EXPR_ERROR;
    }
    




}


U16 lexer_consume_charset(REGEX_SCANNER* lexer, char current){
    //
    //These chars have special meaning within a charset
    //
    //       ]#&|~()
    //
    //
    //
    //
    //
    switch(current){
        case ')':
            return RPAREN;
            break;
        case '(':
            return LPAREN;
            break;
        case ']':
            lexer->is_charset = false;
            return RPAREN;
            break;
        case '~':
            return OP_NOT;
            break;
        case '&':
            return OP_AND;
            break;
        case '|':
            return OP_OR;
            break;
        case '#':
            current = lexer_next_char(lexer);
            return lexer_predef_set(current);
            break;
        case '\\':
            current = lexer_next_char(lexer);
            return lexer_parse_escape(current);
            break;
        default:
            return 1024 + current;
    
    
    
    }


}

U16 lexer_consume_token(REGEX_SCANNER* lexer){
    
    char current = lexer_next_char(lexer);

    while(1) {
        if(current == '\0'){
            return EXPR_END;
        }
        else if(current == ' ' || current == '\t' || current == '\v' || current == '\r' || current == '\n' || current == '\f'){
            current = lexer_next_char(lexer); 
        }
        else {
            break;
        }
    }
    
    if(lexer->is_charset){
        return lexer_consume_charset(lexer, current);
    } else {
        return lexer_consume_regex(lexer, current);
    }
}

U16 lexer_consume(REGEX_SCANNER* lexer){

    // Rule to inject a cat operator
    //
    //  Lexer must not be in charset mode
    //
    //  {operand or rparen or postfix op}{operand or lparen or prefix}
    //
    //
    //
    //

    if(!lexer->previous_mode){
        B32 previous_term_end = (lexer->previous_token > 257 || lexer->previous_token == RPAREN);
        B32 current_term_start = (lexer->current_token > 1023 || lexer->current_token == LPAREN);
        if(previous_term_end && current_term_start){
            U16 ret = lexer->previous_token;
            lexer->previous_token = OP_CAT;
            return ret;
        }
    }

    U16 ret = lexer->previous_token;
    lexer->previous_token = lexer->current_token;
    lexer->current_token = lexer_consume_token(lexer);

    lexer->previous_mode = lexer->current_mode;
    lexer->current_mode = (lexer->is_charset);
    return ret;

}

REGEX_SCANNER lexer_create(STRING8 regex){
    REGEX_SCANNER lex;
    lex.source = regex;
    lex.index = 0;

    lex.is_charset = false;

    lex.previous_token = lexer_consume_token(&lex);
    lex.previous_mode = lex.is_charset;
    lex.current_token = lexer_consume_token(&lex);
    lex.current_mode = lex.is_charset;



    lex.stack_top = 0;
    lex.shunting_yard_stack[0] = lexer_consume(&lex);

    return lex;
}


U16 lexer_consume_postfix(REGEX_SCANNER* scanner){
    
    U16 *stack = scanner->shunting_yard_stack;
    U64 *top = &(scanner->stack_top);
    
    //good thing for us: the operator values are representative of precendence
    //  all associativities are left
    //
    //
    
    //if the top is an operand, we pass it along to the output.
    while(1){
        //stack overflow condition
        if(*top == 511){
            return EXPR_ERROR;
        }

        if(stack[*top] > 767){
            U16 ret = stack[*top];
            stack[*top] = lexer_consume(scanner);
            return ret;
        } 
        
        //if the top is a postfix operator, we simply pass it along
        if(stack[*top] == OP_STAR || stack[*top] == OP_PLUS || stack[*top] == OP_OPT){
            U16 ret = stack[*top];
            stack[*top] = lexer_consume(scanner);
            return ret;
        } 


        //if the top is a termination character, return whatever is below the top of the stack.
        //if top = 0, we just return the termination character.
        if(stack[*top] == EXPR_END || stack[*top] == EXPR_ERROR){
            if(*top == 0){
                return EXPR_END;
            }
            U16 ret = stack[*top - 1];
            stack[*top - 1] = stack[*top];
            (*top)--;
            return ret;
        }

        //if the top is an rparen, we just emit below the stack. Unless the bottom is also an lparen, if so we pop both off, push the next input, and redo the loop.
        if(stack[*top] == RPAREN){
            if(stack[*top - 1] == LPAREN){
                (*top)--;//pop off the top
                stack[*top] = lexer_consume(scanner);//pop and push the next from the input stream.
            } else {
                U16 ret = stack[*top - 1];//take whats below the stack
                stack[*top - 1] = stack[*top];//copy down 1
                (*top)--;//pop the stack
                return ret;
            }
        }

        if(stack[*top] == LPAREN){
            //just push it blindly onto the stack
            (*top)++;
            stack[*top] = lexer_consume(scanner);
        }

        //if the top is a binary operator, we check the bottom to see if we push, or return what's below.
        if(stack[*top] == OP_CAT || stack[*top] == OP_BAR || 
           stack[*top] == OP_AND || stack[*top] == OP_OR || stack[*top] == OP_NOT) {
            
            //condition for push

            //if stack is empty, simply push the current top, and make the top the next token
            if(*top == 0){
                (*top)++;
                stack[*top] = lexer_consume(scanner);
            } else if (stack[*top - 1] == LPAREN){
                //if stack has an LPAREN below the top, we simply push a new top token
                (*top)++;
                stack[*top] = lexer_consume(scanner);
            } else if(stack[*top] > stack[*top - 1]){
                //if the top has higher precedence, we can push
                (*top)++;
                stack[*top] = lexer_consume(scanner);
            } else if(stack[*top] <= stack[*top - 1]){
                //if the precendence is lower or equal, we pop below the stack
                U16 ret = stack[*top - 1];
                stack[*top - 1] = stack[*top];
                (*top)--;
                return ret;
            }
        }
    }
}






