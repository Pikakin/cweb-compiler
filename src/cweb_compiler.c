/**
 * CWeb Compiler - CWeb言語をJavaScriptに変換するコンパイラ
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include "cweb_parser.h"

// グローバル変数
char* outputBuffer = NULL;
int outputPos = 0;
Token* tokens = NULL;
int tokenCount = 0;
int currentToken = 0;
Component* components = NULL;
int componentCount = 0;

char* currentSourceFile = NULL;
char** sourceLines = NULL;
int sourceLineCount = 0;

/**
 * ファイルの内容を読み込む
 */
char* readFile(const char* filePath) {
    FILE* file = fopen(filePath, "r");
    if (!file) {
        printf("エラー: ファイル '%s' を開けませんでした\n", filePath);
        return NULL;
    }
    
    // ファイルサイズを取得
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // メモリを確保してファイル内容を読み込む
    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) {
        printf("エラー: メモリを確保できませんでした\n");
        fclose(file);
        return NULL;
    }
    
    size_t bytesRead = fread(buffer, 1, fileSize, file);
    buffer[bytesRead] = '\0';
    
    fclose(file);
    return buffer;
}

/**
 * エラーメッセージを表示
 */
void printError(const char* message, int line, int column) {
    fprintf(stderr, "\033[1;31mエラー\033[0m (行 %d, 列 %d): %s\n", line, column, message);
    
    // ファイル名と該当行を表示
    if (currentSourceFile != NULL) {
        fprintf(stderr, "ファイル: %s\n", currentSourceFile);
        
        // 該当行の内容を表示（可能であれば）
        if (sourceLines != NULL && line <= sourceLineCount) {
            fprintf(stderr, "%4d | %s\n", line, sourceLines[line-1]);
            
            // エラー位置を示す矢印を表示
            fprintf(stderr, "     | ");
            for (int i = 1; i < column; i++) {
                fprintf(stderr, " ");
            }
            fprintf(stderr, "\033[1;31m^\033[0m\n");
        }
    }
    
    exit(1);
}

/**
 * ソースコードの行を保存する関数
 */
void saveSourceLines(const char* source) {
    // 既存の行を解放
    if (sourceLines != NULL) {
        for (int i = 0; i < sourceLineCount; i++) {
            free(sourceLines[i]);
        }
        free(sourceLines);
    }
    
    // 行数をカウント
    sourceLineCount = 1;
    for (int i = 0; source[i] != '\0'; i++) {
        if (source[i] == '\n') {
            sourceLineCount++;
        }
    }
    
    // 行の配列を確保
    sourceLines = (char**)malloc(sizeof(char*) * sourceLineCount);
    if (sourceLines == NULL) {
        fprintf(stderr, "メモリ割り当てエラー: sourceLines\n");
        exit(1);
    }
    
    // 各行を保存
    int lineStart = 0;
    int lineIndex = 0;
    for (int i = 0; source[i] != '\0'; i++) {
        if (source[i] == '\n') {
            int lineLength = i - lineStart;
            sourceLines[lineIndex] = (char*)malloc(lineLength + 1);
            if (sourceLines[lineIndex] == NULL) {
                fprintf(stderr, "メモリ割り当てエラー: sourceLines[%d]\n", lineIndex);
                exit(1);
            }
            strncpy(sourceLines[lineIndex], source + lineStart, lineLength);
            sourceLines[lineIndex][lineLength] = '\0';
            lineStart = i + 1;
            lineIndex++;
        }
    }
    
    // 最後の行を保存
    if (lineStart < (int)strlen(source)) {
        int lineLength = strlen(source) - lineStart;
        sourceLines[lineIndex] = (char*)malloc(lineLength + 1);
        if (sourceLines[lineIndex] == NULL) {
            fprintf(stderr, "メモリ割り当てエラー: sourceLines[%d]\n", lineIndex);
            exit(1);
        }
        strncpy(sourceLines[lineIndex], source + lineStart, lineLength);
        sourceLines[lineIndex][lineLength] = '\0';
    }
}

/**
 * トークンタイプ名を取得する関数
 */
const char* getTokenTypeName(TokenType type) {
    switch (type) {
        case TOKEN_COMPONENT: return "COMPONENT";
        case TOKEN_STATE: return "STATE";
        case TOKEN_ACTION: return "ACTION";
        case TOKEN_DISPLAY: return "DISPLAY";
        case TOKEN_DATA: return "DATA";
        case TOKEN_ASYNC: return "ASYNC";
        case TOKEN_AWAIT: return "AWAIT";
        case TOKEN_TRY: return "TRY";
        case TOKEN_CATCH: return "CATCH";
        case TOKEN_FINALLY: return "FINALLY";
        case TOKEN_ON_LOAD: return "ON_LOAD";
        case TOKEN_SHOW_WHEN: return "SHOW_WHEN";
        case TOKEN_FOR_EACH: return "FOR_EACH";
        case TOKEN_STYLES: return "STYLES";
        case TOKEN_EXPORT: return "EXPORT";
        case TOKEN_IDENTIFIER: return "識別子";
        case TOKEN_STRING: return "文字列";
        case TOKEN_NUMBER: return "数値";
        case TOKEN_EQUALS: return "=";
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_MULTIPLY: return "*";
        case TOKEN_DIVIDE: return "/";
        case TOKEN_OPEN_PAREN: return "(";
        case TOKEN_CLOSE_PAREN: return ")";
        case TOKEN_OPEN_BRACE: return "{";
        case TOKEN_CLOSE_BRACE: return "}";
        case TOKEN_OPEN_BRACKET: return "[";
        case TOKEN_CLOSE_BRACKET: return "]";
        case TOKEN_COMMA: return ",";
        case TOKEN_SEMICOLON: return ";";
        case TOKEN_DOT: return ".";
        case TOKEN_ARROW: return "->";
        case TOKEN_HTML_TAG: return "HTMLタグ";
        case TOKEN_EOF: return "ファイル終端";
        default: return "不明なトークン";
    }
}

/**
 * 現在のトークンが指定された型であることを期待し、そうでなければエラー
 */
bool expect(TokenType type, const char* message) {
    if (consume(type)) {
        return true;
    }
    
    char errorMsg[256];
    sprintf(errorMsg, "%s\n期待: %s\n実際: %s", 
            message, 
            getTokenTypeName(type), 
            getTokenTypeName(tokens[currentToken].type));
    printError(errorMsg, tokens[currentToken].line, tokens[currentToken].column);
    return false;
}

/**
 * 識別子かどうかを判定
 */
bool isIdentifier(char c) {
    return isalnum(c) || c == '_';
}

/**
 * ソースコードをトークンに分割
 */
void tokenize(const char* source) {
    int line = 1;
    int column = 1;
    int i = 0;
    
    while (source[i] != '\0') {
        // 空白文字をスキップ
        if (isspace(source[i])) {
            if (source[i] == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
            i++;
            continue;
        }
        
        // コメントをスキップ
        if (source[i] == '/' && source[i+1] == '/') {
            i += 2;
            while (source[i] != '\0' && source[i] != '\n') {
                i++;
            }
            continue;
        }
        
        // キーワードまたは識別子
        if (isalpha(source[i]) || source[i] == '_') {
            char buffer[MAX_TOKEN_LENGTH];
            int bufPos = 0;
            
            while (isIdentifier(source[i])) {
                buffer[bufPos++] = source[i++];
            }
            buffer[bufPos] = '\0';
            
            TokenType type;
            if (strcmp(buffer, "COMPONENT") == 0) {
                type = TOKEN_COMPONENT;
            } else if (strcmp(buffer, "STATE") == 0) {
                type = TOKEN_STATE;
            } else if (strcmp(buffer, "ACTION") == 0) {
                type = TOKEN_ACTION;
            } else if (strcmp(buffer, "DISPLAY") == 0) {
                type = TOKEN_DISPLAY;
            } else if (strcmp(buffer, "DATA") == 0) {
                type = TOKEN_DATA;
            } else if (strcmp(buffer, "ASYNC") == 0) {
                type = TOKEN_ASYNC;
            } else if (strcmp(buffer, "AWAIT") == 0) {
                type = TOKEN_AWAIT;
            } else if (strcmp(buffer, "TRY") == 0) {
                type = TOKEN_TRY;
            } else if (strcmp(buffer, "CATCH") == 0) {
                type = TOKEN_CATCH;
            } else if (strcmp(buffer, "FINALLY") == 0) {
                type = TOKEN_FINALLY;
            } else if (strcmp(buffer, "ON_LOAD") == 0) {
                type = TOKEN_ON_LOAD;
            } else if (strcmp(buffer, "SHOW_WHEN") == 0) {
                type = TOKEN_SHOW_WHEN;
            } else if (strcmp(buffer, "FOR_EACH") == 0) {
                type = TOKEN_FOR_EACH;
            } else if (strcmp(buffer, "STYLES") == 0) {
                type = TOKEN_STYLES;
            } else if (strcmp(buffer, "EXPORT") == 0) {
                type = TOKEN_EXPORT;
            } else {
                type = TOKEN_IDENTIFIER;
            }
            
            tokens[tokenCount].type = type;
            strcpy(tokens[tokenCount].value, buffer);
            tokens[tokenCount].line = line;
            tokens[tokenCount].column = column;
            tokenCount++;
            
            column += bufPos;
            continue;
        }
        
        // 文字列リテラル
        if (source[i] == '"' || source[i] == '\'') {
            char quote = source[i++];
            char buffer[MAX_TOKEN_LENGTH];
            int bufPos = 0;
            
            while (source[i] != '\0' && source[i] != quote) {
                if (source[i] == '\\' && source[i+1] != '\0') {
                    buffer[bufPos++] = source[i++]; // バックスラッシュを保持
                }
                buffer[bufPos++] = source[i++];
            }
            
            if (source[i] == '\0') {
                printError("文字列が閉じられていません", line, column);
            }
            
            buffer[bufPos] = '\0';
            i++; // 閉じる引用符をスキップ
            
            tokens[tokenCount].type = TOKEN_STRING;
            strcpy(tokens[tokenCount].value, buffer);
            tokens[tokenCount].line = line;
            tokens[tokenCount].column = column;
            tokenCount++;
            
            column += bufPos + 2; // 引用符を含む
            continue;
        }
        
        // 数値リテラル
        if (isdigit(source[i])) {
            char buffer[MAX_TOKEN_LENGTH];
            int bufPos = 0;
            
            while (isdigit(source[i]) || source[i] == '.') {
                buffer[bufPos++] = source[i++];
            }
            buffer[bufPos] = '\0';
            
            tokens[tokenCount].type = TOKEN_NUMBER;
            strcpy(tokens[tokenCount].value, buffer);
            tokens[tokenCount].line = line;
            tokens[tokenCount].column = column;
            tokenCount++;
            
            column += bufPos;
            continue;
        }
        
        // 単一文字または複合演算子
        TokenType type;
        int advance = 1;
        
        switch (source[i]) {
            case '!':
                if (source[i+1] == '=') {
                    if (source[i+2] == '=') {
                        type = TOKEN_STRICT_NOT_EQUALS;
                        advance = 3;
                    } else {
                        type = TOKEN_NOT_EQUALS;
                        advance = 2;
                    }
                } else {
                    type = TOKEN_NOT;
                }
                break;
            case '=':
                if (source[i+1] == '>') {
                    type = TOKEN_DOUBLE_ARROW;
                    advance = 2;
                } else if (source[i+1] == '=') {
                    if (source[i+2] == '=') {
                        type = TOKEN_STRICT_EQUALS;
                        advance = 3;
                    } else {
                        type = TOKEN_EQUALS_EQUALS;
                        advance = 2;
                    }
                } else {
                    type = TOKEN_EQUALS;
                }
                break;
            case '+':
                if (source[i+1] == '=') {
                    type = TOKEN_PLUS_EQUALS;
                    advance = 2;
                } else if (source[i+1] == '+') {
                    type = TOKEN_PLUS_PLUS;
                    advance = 2;
                } else {
                    type = TOKEN_PLUS;
                }
                break;
            case '-':
                if (source[i+1] == '>') {
                    type = TOKEN_ARROW;
                    advance = 2;
                } else if (source[i+1] == '=') {
                    type = TOKEN_MINUS_EQUALS;
                    advance = 2;
                } else if (source[i+1] == '-') {
                    type = TOKEN_MINUS_MINUS;
                    advance = 2;
                } else {
                    type = TOKEN_MINUS;
                }
                break;
            case '*':
                if (source[i+1] == '=') {
                    type = TOKEN_MULTIPLY_EQUALS;
                    advance = 2;
                } else {
                    type = TOKEN_MULTIPLY;
                }
                break;
            case '/':
                if (source[i+1] == '=') {
                    type = TOKEN_DIVIDE_EQUALS;
                    advance = 2;
                } else {
                    type = TOKEN_DIVIDE;
                }
                break;
            case '%':
                if (source[i+1] == '=') {
                    type = TOKEN_MODULO_EQUALS;
                    advance = 2;
                } else {
                    type = TOKEN_MODULO;
                }
                break;
            case '&':
                if (source[i+1] == '&') {
                    type = TOKEN_AND;
                    advance = 2;
                } else {
                    type = TOKEN_BITWISE_AND;
                }
                break;
            case '|':
                if (source[i+1] == '|') {
                    type = TOKEN_OR;
                    advance = 2;
                } else {
                    type = TOKEN_BITWISE_OR;
                }
                break;
            case '^':
                type = TOKEN_BITWISE_XOR;
                break;
            case '~':
                type = TOKEN_BITWISE_NOT;
                break;
            case '<':
                if (source[i+1] == '<') {
                    type = TOKEN_SHIFT_LEFT;
                    advance = 2;
                } else if (isalpha(source[i+1])) {
                    // HTMLタグの処理
                    char buffer[MAX_TOKEN_LENGTH];
                    int bufPos = 0;
                    i++; // '<' をスキップ
                    
                    while (isIdentifier(source[i])) {
                        buffer[bufPos++] = source[i++];
                    }
                    buffer[bufPos] = '\0';
                    
                    tokens[tokenCount].type = TOKEN_HTML_TAG;
                    strcpy(tokens[tokenCount].value, buffer);
                    tokens[tokenCount].line = line;
                    tokens[tokenCount].column = column;
                    tokenCount++;
                    
                    column += bufPos + 1; // '<' を含む
                    continue;
                } else {
                    printError("無効な文字です", line, column);
                }
                break;
            case '>':
                if (source[i+1] == '>' && source[i+2] == '>') {
                    type = TOKEN_UNSIGNED_SHIFT_RIGHT;
                    advance = 3;
                } else if (source[i+1] == '>') {
                    type = TOKEN_SHIFT_RIGHT;
                    advance = 2;
                } else if (source[i+1] == '=') {
                    type = TOKEN_GREATER_THAN_EQUALS;
                    advance = 2;
                } else {
                    type = TOKEN_GREATER_THAN;
                    advance = 1;
                }
                break;
            case '#':
                type = TOKEN_HASH;
                break;
            case '?':
                if (source[i+1] == '?') {
                    type = TOKEN_NULLISH_COALESCING;
                    advance = 2;
                } else if (source[i+1] == '.') {
                    type = TOKEN_OPTIONAL_CHAINING;
                    advance = 2;
                } else {
                    type = TOKEN_QUESTION;
                }
                break;
            case ':':
                type = TOKEN_COLON;
                break;
            case '.':
                if (source[i+1] == '.' && source[i+2] == '.') {
                    type = TOKEN_SPREAD;
                    advance = 3;
                } else {
                    type = TOKEN_DOT;
                }
                break;
            case '(':
                type = TOKEN_OPEN_PAREN;
                break;
            case ')':
                type = TOKEN_CLOSE_PAREN;
                break;
            case '{':
                type = TOKEN_OPEN_BRACE;
                break;
            case '}':
                type = TOKEN_CLOSE_BRACE;
                break;
            case '[':
                type = TOKEN_OPEN_BRACKET;
                break;
            case ']':
                type = TOKEN_CLOSE_BRACKET;
                break;
            case ',':
                type = TOKEN_COMMA;
                break;
            case ';':
                type = TOKEN_SEMICOLON;
                break;
            default:
                {
                    char errorMsg[100];
                    if (isprint(source[i])) {
                        sprintf(errorMsg, "無効な文字です: '%c' (ASCII: %d)", source[i], (int)source[i]);
                    } else {
                        sprintf(errorMsg, "無効な文字です: (ASCII: %d)", (int)source[i]);
                    }
                    printError(errorMsg, line, column);
                }
                break;
        }
        
        tokens[tokenCount].type = type;
        strncpy(tokens[tokenCount].value, source + i, advance);
        tokens[tokenCount].value[advance] = '\0';
        tokens[tokenCount].line = line;
        tokens[tokenCount].column = column;
        tokenCount++;
        
        i += advance;
        column += advance;
    }
    
    // EOFトークンを追加
    tokens[tokenCount].type = TOKEN_EOF;
    tokens[tokenCount].value[0] = '\0';
    tokens[tokenCount].line = line;
    tokens[tokenCount].column = column;
    tokenCount++;
    
    printf("トークン化完了: %d トークンを生成しました\n", tokenCount);
}

/**
 * 現在のトークンが指定された型かどうかを確認
 */
bool match(TokenType type) {
    return tokens[currentToken].type == type;
}

/**
 * 現在のトークンが指定された型なら消費して true を返す
 */
bool consume(TokenType type) {
    if (match(type)) {
        currentToken++;
        return true;
    }
    return false;
}

/**
 * 状態変数の型を文字列から解析
 */
StateType parseStateType(const char* typeStr) {
    if (strcmp(typeStr, "String") == 0) {
        return TYPE_STRING;
    } else if (strcmp(typeStr, "Number") == 0) {
        return TYPE_NUMBER;
    } else if (strcmp(typeStr, "Boolean") == 0) {
        return TYPE_BOOLEAN;
    } else if (strcmp(typeStr, "Array") == 0) {
        return TYPE_ARRAY;
    } else if (strcmp(typeStr, "Object") == 0) {
        return TYPE_OBJECT;
    } else {
        return TYPE_ANY;
    }
}

/**
 * 状態変数の型を文字列に変換
 */
char* getStateTypeString(StateType type) {
    switch (type) {
        case TYPE_STRING: return "String";
        case TYPE_NUMBER: return "Number";
        case TYPE_BOOLEAN: return "Boolean";
        case TYPE_ARRAY: return "Array";
        case TYPE_OBJECT: return "Object";
        case TYPE_ANY: default: return "Any";
    }
}

/**
 * 状態変数を解析
 */
void parseState(Component* component) {
    // STATE キーワードを消費
    expect(TOKEN_STATE, "状態変数定義が必要です");
    
    // 状態変数名
    expect(TOKEN_IDENTIFIER, "状態変数名が必要です");
    strcpy(component->states[component->stateCount].name, tokens[currentToken - 1].value);
    
    // 型（オプション）
    if (match(TOKEN_IDENTIFIER)) {
        StateType type = parseStateType(tokens[currentToken].value);
        component->states[component->stateCount].type = type;
        currentToken++;
    } else {
        component->states[component->stateCount].type = TYPE_ANY;
    }
    
    // 初期値（オプション）
    if (consume(TOKEN_EQUALS)) {
        // 初期値を解析
        if (match(TOKEN_STRING)) {
            strcpy(component->states[component->stateCount].initialValue, tokens[currentToken].value);
            currentToken++;
        } else if (match(TOKEN_NUMBER)) {
            strcpy(component->states[component->stateCount].initialValue, tokens[currentToken].value);
            currentToken++;
        } else if (match(TOKEN_IDENTIFIER)) {
            strcpy(component->states[component->stateCount].initialValue, tokens[currentToken].value);
            currentToken++;
        } else if (match(TOKEN_OPEN_BRACKET)) {
            // 配列リテラル
            strcpy(component->states[component->stateCount].initialValue, "[]");
            currentToken++;
            
            // 配列の内容はスキップ
            int bracketCount = 1;
            while (bracketCount > 0 && !match(TOKEN_EOF)) {
                if (match(TOKEN_OPEN_BRACKET)) {
                    bracketCount++;
                } else if (match(TOKEN_CLOSE_BRACKET)) {
                    bracketCount--;
                }
                currentToken++;
            }
        } else if (match(TOKEN_OPEN_BRACE)) {
            // オブジェクトリテラル
            strcpy(component->states[component->stateCount].initialValue, "{}");
            currentToken++;
            
            // オブジェクトの内容はスキップ
            int braceCount = 1;
            while (braceCount > 0 && !match(TOKEN_EOF)) {
                if (match(TOKEN_OPEN_BRACE)) {
                    braceCount++;
                } else if (match(TOKEN_CLOSE_BRACE)) {
                    braceCount--;
                }
                currentToken++;
            }
        } else {
            strcpy(component->states[component->stateCount].initialValue, "null");
        }
    } else {
        // 初期値が指定されていない場合はデフォルト値を設定
        switch (component->states[component->stateCount].type) {
            case TYPE_STRING:
                strcpy(component->states[component->stateCount].initialValue, "\"\"");
                break;
            case TYPE_NUMBER:
                strcpy(component->states[component->stateCount].initialValue, "0");
                break;
            case TYPE_BOOLEAN:
                strcpy(component->states[component->stateCount].initialValue, "false");
                break;
            case TYPE_ARRAY:
                strcpy(component->states[component->stateCount].initialValue, "[]");
                break;
            case TYPE_OBJECT:
                strcpy(component->states[component->stateCount].initialValue, "{}");
                break;
            case TYPE_ANY:
            default:
                strcpy(component->states[component->stateCount].initialValue, "null");
                break;
        }
    }
    
    // セミコロンを期待
    expect(TOKEN_SEMICOLON, "状態変数定義の終了には ';' が必要です");
    
    component->stateCount++;
}

/**
 * データ宣言を解析
 */
void parseData(Component* component) {
    // DATA キーワードを消費
    expect(TOKEN_DATA, "データ宣言が必要です");
    
    // データ名
    expect(TOKEN_IDENTIFIER, "データ名が必要です");
    strcpy(component->data[component->dataCount].name, tokens[currentToken - 1].value);
    
    // 等号を期待
    expect(TOKEN_EQUALS, "データ宣言には '=' が必要です");
    
    // オブジェクトリテラルを期待
    expect(TOKEN_OPEN_BRACE, "データ宣言にはオブジェクトリテラルが必要です");
    
    // オブジェクトの内容を解析
    while (!match(TOKEN_CLOSE_BRACE) && !match(TOKEN_EOF)) {
        // フィールド名
        if (match(TOKEN_IDENTIFIER)) {
            strcpy(component->data[component->dataCount].fields[component->data[component->dataCount].fieldCount].name, tokens[currentToken].value);
            currentToken++;
        } else {
            printError("データフィールド名が必要です", tokens[currentToken].line, tokens[currentToken].column);
        }
        
        // コロンを期待
        expect(TOKEN_COLON, "データフィールドには ':' が必要です");
        
        // フィールド値
        if (match(TOKEN_STRING)) {
            strcpy(component->data[component->dataCount].fields[component->data[component->dataCount].fieldCount].value, tokens[currentToken].value);
            currentToken++;
        } else if (match(TOKEN_NUMBER)) {
            strcpy(component->data[component->dataCount].fields[component->data[component->dataCount].fieldCount].value, tokens[currentToken].value);
            currentToken++;
        } else if (match(TOKEN_IDENTIFIER)) {
            strcpy(component->data[component->dataCount].fields[component->data[component->dataCount].fieldCount].value, tokens[currentToken].value);
            currentToken++;
        } else if (match(TOKEN_OPEN_BRACKET) || match(TOKEN_OPEN_BRACE)) {
            // 配列またはオブジェクトリテラル
            TokenType openToken = tokens[currentToken].type;
            TokenType closeToken = (openToken == TOKEN_OPEN_BRACKET) ? TOKEN_CLOSE_BRACKET : TOKEN_CLOSE_BRACE;
            
            strcpy(component->data[component->dataCount].fields[component->data[component->dataCount].fieldCount].value, 
                  (openToken == TOKEN_OPEN_BRACKET) ? "[]" : "{}");
            currentToken++;
            
            // 内容はスキップ
            int nestCount = 1;
            while (nestCount > 0 && !match(TOKEN_EOF)) {
                if (match(openToken)) {
                    nestCount++;
                } else if (match(closeToken)) {
                    nestCount--;
                }
                currentToken++;
            }
        } else {
            strcpy(component->data[component->dataCount].fields[component->data[component->dataCount].fieldCount].value, "null");
        }
        
        component->data[component->dataCount].fieldCount++;
        
        // カンマがあれば次のフィールドへ
        if (consume(TOKEN_COMMA)) {
            continue;
        }
    }
    
    expect(TOKEN_CLOSE_BRACE, "データ宣言の終了には '}' が必要です");
    expect(TOKEN_SEMICOLON, "データ宣言の終了には ';' が必要です");
    
    component->dataCount++;
}

/**
 * アクション関数を解析
 */
void parseAction(Component* component) {
    // ASYNC キーワードがあるか確認
    bool isAsync = consume(TOKEN_ASYNC);
    
    // ACTION キーワードを消費
    expect(TOKEN_ACTION, "アクション定義が必要です");
    
    // アクション名
    expect(TOKEN_IDENTIFIER, "アクション名が必要です");
    strcpy(component->actions[component->actionCount].name, tokens[currentToken - 1].value);
    component->actions[component->actionCount].isAsync = isAsync;
    
    // パラメータリスト
    expect(TOKEN_OPEN_PAREN, "パラメータリストの開始には '(' が必要です");
    
    // パラメータを解析
    while (!match(TOKEN_CLOSE_PAREN) && !match(TOKEN_EOF)) {
        if (match(TOKEN_IDENTIFIER)) {
            strcpy(component->actions[component->actionCount].params[component->actions[component->actionCount].paramCount].name, tokens[currentToken].value);
            currentToken++;
            
            // 型指定があれば解析（オプション）
            if (consume(TOKEN_COLON)) {
                if (match(TOKEN_IDENTIFIER)) {
                    strcpy(component->actions[component->actionCount].params[component->actions[component->actionCount].paramCount].type, tokens[currentToken].value);
                    currentToken++;
                }
            } else {
                strcpy(component->actions[component->actionCount].params[component->actions[component->actionCount].paramCount].type, "any");
            }
            
            component->actions[component->actionCount].paramCount++;
        }
        
        // カンマがあれば次のパラメータへ
        if (consume(TOKEN_COMMA)) {
            continue;
        } else {
            break;
        }
    }
    
    expect(TOKEN_CLOSE_PAREN, "パラメータリストの終了には ')' が必要です");
    
    // アクション本体
    expect(TOKEN_OPEN_BRACE, "アクション本体の開始には '{' が必要です");
    
    // アクション本体のコードを収集
    int startPos = currentToken;
    int braceCount = 1;
    
    while (braceCount > 0 && !match(TOKEN_EOF)) {
        currentToken++;
        if (match(TOKEN_OPEN_BRACE)) {
            braceCount++;
        } else if (match(TOKEN_CLOSE_BRACE)) {
            braceCount--;
        }
    }
    
    int endPos = currentToken;
    expect(TOKEN_CLOSE_BRACE, "アクション本体の終了には '}' が必要です");
    
    // アクション本体のコードを文字列として保存
    component->actions[component->actionCount].body[0] = '\0';
    for (int i = startPos; i < endPos; i++) {
        strcat(component->actions[component->actionCount].body, tokens[i].value);
        strcat(component->actions[component->actionCount].body, " ");
    }
    
    component->actionCount++;
}

/**
 * ON_LOAD ブロックを解析
 */
void parseOnLoad(Component* component) {
    // ON_LOAD キーワードを消費
    expect(TOKEN_ON_LOAD, "ON_LOAD ブロックが必要です");
    
    // ブロック本体
    expect(TOKEN_OPEN_BRACE, "ON_LOAD ブロックの開始には '{' が必要です");
    
    // ブロック本体のコードを収集
    int startPos = currentToken;
    int braceCount = 1;
    
    while (braceCount > 0 && !match(TOKEN_EOF)) {
        currentToken++;
        if (match(TOKEN_OPEN_BRACE)) {
            braceCount++;
        } else if (match(TOKEN_CLOSE_BRACE)) {
            braceCount--;
        }
    }
    
    int endPos = currentToken;
    expect(TOKEN_CLOSE_BRACE, "ON_LOAD ブロックの終了には '}' が必要です");
    
    // ON_LOAD ブロックのコードを文字列として保存
    component->onLoadCode[0] = '\0';
    for (int i = startPos; i < endPos; i++) {
        strcat(component->onLoadCode, tokens[i].value);
        strcat(component->onLoadCode, " ");
    }
    
    component->hasOnLoad = true;
}

/**
 * JSX属性を解析
 */
void parseJSXAttributes(JSXElement* element) {
    while (!match(TOKEN_CLOSE_PAREN) && !match(TOKEN_EOF)) {
        // 属性名
        if (match(TOKEN_IDENTIFIER)) {
            strcpy(element->attributes[element->attributeCount].name, tokens[currentToken].value);
            currentToken++;
            
            // 等号を期待
            expect(TOKEN_EQUALS, "JSX属性には '=' が必要です");
            
            // 属性値
            if (match(TOKEN_STRING)) {
                strcpy(element->attributes[element->attributeCount].value, tokens[currentToken].value);
                element->attributes[element->attributeCount].isExpression = false;
                currentToken++;
            } else if (match(TOKEN_OPEN_BRACE)) {
                // 式として解析
                currentToken++;
                
                // 式の内容を収集
                int startPos = currentToken;
                int braceCount = 1;
                
                while (braceCount > 0 && !match(TOKEN_EOF)) {
                    if (match(TOKEN_OPEN_BRACE)) {
                        braceCount++;
                    } else if (match(TOKEN_CLOSE_BRACE)) {
                        braceCount--;
                    }
                    
                    if (braceCount > 0) {
                        currentToken++;
                    }
                }
                
                int endPos = currentToken;
                expect(TOKEN_CLOSE_BRACE, "式の終了には '}' が必要です");
                
                // 式を文字列として保存
                element->attributes[element->attributeCount].value[0] = '\0';
                for (int i = startPos; i < endPos; i++) {
                    strcat(element->attributes[element->attributeCount].value, tokens[i].value);
                    strcat(element->attributes[element->attributeCount].value, " ");
                }
                
                element->attributes[element->attributeCount].isExpression = true;
            }
            
            element->attributeCount++;
        } else {
            printError("JSX属性名が必要です", tokens[currentToken].line, tokens[currentToken].column);
        }
        
        // カンマがあれば次の属性へ
        if (consume(TOKEN_COMMA)) {
            continue;
        } else {
            break;
        }
    }
}

/**
 * SHOW_WHEN ブロックを解析
 */
void parseShowWhen(Component* component, int parentIndex) {
    // SHOW_WHEN キーワードを消費
    expect(TOKEN_SHOW_WHEN, "SHOW_WHEN ブロックが必要です");
    
    // 条件式を解析
    expect(TOKEN_OPEN_PAREN, "条件式の開始には '(' が必要です");
    
    // 条件式の内容を収集
    int startPos = currentToken;
    int parenCount = 1;
    
    while (parenCount > 0 && !match(TOKEN_EOF)) {
        if (match(TOKEN_OPEN_PAREN)) {
            parenCount++;
        } else if (match(TOKEN_CLOSE_PAREN)) {
            parenCount--;
        }
        
        if (parenCount > 0) {
            currentToken++;
        }
    }
    
    int endPos = currentToken;
    expect(TOKEN_CLOSE_PAREN, "条件式の終了には ')' が必要です");
    
    // 条件式を文字列として保存
    char condition[MAX_TOKEN_LENGTH * 10] = "";
    for (int i = startPos; i < endPos; i++) {
        strcat(condition, tokens[i].value);
        strcat(condition, " ");
    }
    
    // SHOW_WHEN ブロックの本体
    expect(TOKEN_OPEN_BRACE, "SHOW_WHEN ブロックの開始には '{' が必要です");
    
    // 条件付きレンダリングのための特殊なJSX要素を作成
    int elementIndex = component->jsxElementCount;
    strcpy(component->jsxElements[elementIndex].tagName, "ConditionalRender");
    component->jsxElements[elementIndex].parentIndex = parentIndex;
    component->jsxElements[elementIndex].childCount = 0;
    component->jsxElements[elementIndex].attributeCount = 1;
    component->jsxElements[elementIndex].hasTextContent = false;
    
    // 条件を属性として追加
    strcpy(component->jsxElements[elementIndex].attributes[0].name, "condition");
    strcpy(component->jsxElements[elementIndex].attributes[0].value, condition);
    component->jsxElements[elementIndex].attributes[0].isExpression = true;
    
    // ブロック内のJSX要素を解析
    while (!match(TOKEN_CLOSE_BRACE) && !match(TOKEN_EOF)) {
        if (match(TOKEN_IDENTIFIER) || match(TOKEN_HTML_TAG)) {
            int childIndex = parseJSXElement(component, elementIndex);
            component->jsxElements[elementIndex].childrenIndices[component->jsxElements[elementIndex].childCount++] = childIndex;
        } else {
            // その他のトークンはスキップ
            currentToken++;
        }
    }
    
    expect(TOKEN_CLOSE_BRACE, "SHOW_WHEN ブロックの終了には '}' が必要です");
    
    // 親要素に条件付きレンダリング要素を追加
    component->jsxElements[parentIndex].childrenIndices[component->jsxElements[parentIndex].childCount++] = elementIndex;
    component->jsxElementCount++;
}

/**
 * FOR_EACH ブロックを解析
 */
void parseForEach(Component* component, int parentIndex) {
    // FOR_EACH キーワードを消費
    expect(TOKEN_FOR_EACH, "FOR_EACH ブロックが必要です");
    
    // 反復式を解析
    expect(TOKEN_OPEN_PAREN, "反復式の開始には '(' が必要です");
    
    // 反復変数名を取得
    expect(TOKEN_IDENTIFIER, "反復変数名が必要です");
    char iteratorVar[MAX_TOKEN_LENGTH];
    strcpy(iteratorVar, tokens[currentToken - 1].value);
    
    // "in" キーワードを期待
    // 注意: CWebの構文によっては、"in"が特別なトークンタイプである可能性があります
    if (match(TOKEN_IDENTIFIER) && strcmp(tokens[currentToken].value, "in") == 0) {
        currentToken++;
    } else {
        printError("FOR_EACH 構文には 'in' キーワードが必要です", tokens[currentToken].line, tokens[currentToken].column);
    }
    
    // コレクション式の内容を収集
    int startPos = currentToken;
    int parenCount = 0;
    
    while ((parenCount > 0 || !match(TOKEN_CLOSE_PAREN)) && !match(TOKEN_EOF)) {
        if (match(TOKEN_OPEN_PAREN)) {
            parenCount++;
        } else if (match(TOKEN_CLOSE_PAREN)) {
            parenCount--;
            if (parenCount < 0) {
                break;
            }
        }
        currentToken++;
    }
    
    int endPos = currentToken;
    expect(TOKEN_CLOSE_PAREN, "反復式の終了には ')' が必要です");
    
    // コレクション式を文字列として保存
    char collection[MAX_TOKEN_LENGTH * 10] = "";
    for (int i = startPos; i < endPos; i++) {
        strcat(collection, tokens[i].value);
        strcat(collection, " ");
    }
    
    // FOR_EACH ブロックの本体
    expect(TOKEN_OPEN_BRACE, "FOR_EACH ブロックの開始には '{' が必要です");
    
    // 反復レンダリングのための特殊なJSX要素を作成
    int elementIndex = component->jsxElementCount;
    strcpy(component->jsxElements[elementIndex].tagName, "IterativeRender");
    component->jsxElements[elementIndex].parentIndex = parentIndex;
    component->jsxElements[elementIndex].childCount = 0;
    component->jsxElements[elementIndex].attributeCount = 2;
    component->jsxElements[elementIndex].hasTextContent = false;
    
    // 反復変数と配列を属性として追加
    strcpy(component->jsxElements[elementIndex].attributes[0].name, "iterator");
    strcpy(component->jsxElements[elementIndex].attributes[0].value, iteratorVar);
    component->jsxElements[elementIndex].attributes[0].isExpression = false;
    
    strcpy(component->jsxElements[elementIndex].attributes[1].name, "collection");
    strcpy(component->jsxElements[elementIndex].attributes[1].value, collection);
    component->jsxElements[elementIndex].attributes[1].isExpression = true;
    
    // ブロック内のJSX要素を解析
    while (!match(TOKEN_CLOSE_BRACE) && !match(TOKEN_EOF)) {
        if (match(TOKEN_IDENTIFIER) || match(TOKEN_HTML_TAG)) {
            int childIndex = parseJSXElement(component, elementIndex);
            component->jsxElements[elementIndex].childrenIndices[component->jsxElements[elementIndex].childCount++] = childIndex;
        } else {
            // その他のトークンはスキップ
            currentToken++;
        }
    }
    
    expect(TOKEN_CLOSE_BRACE, "FOR_EACH ブロックの終了には '}' が必要です");
    
    // 親要素に反復レンダリング要素を追加
    component->jsxElements[parentIndex].childrenIndices[component->jsxElements[parentIndex].childCount++] = elementIndex;
    component->jsxElementCount++;
}

/**
 * JSX要素を解析
 */
int parseJSXElement(Component* component, int parentIndex) {
    // タグ名
    if (!match(TOKEN_IDENTIFIER) && !match(TOKEN_HTML_TAG)) {
        printError("JSXタグ名が必要です", tokens[currentToken].line, tokens[currentToken].column);
    }
    
    int elementIndex = component->jsxElementCount;
    strcpy(component->jsxElements[elementIndex].tagName, tokens[currentToken].value);
    component->jsxElements[elementIndex].parentIndex = parentIndex;
    component->jsxElements[elementIndex].childCount = 0;
    component->jsxElements[elementIndex].attributeCount = 0;
    component->jsxElements[elementIndex].hasTextContent = false;
    currentToken++;
    
    // 属性リスト（オプション）
    if (consume(TOKEN_OPEN_PAREN)) {
        parseJSXAttributes(&component->jsxElements[elementIndex]);
        expect(TOKEN_CLOSE_PAREN, "属性リストの終了には ')' が必要です");
    }
    
    // 子要素
    if (consume(TOKEN_OPEN_BRACE)) {  // 中括弧があれば消費
        // 子要素がテキストの場合
        if (match(TOKEN_STRING)) {
            strcpy(component->jsxElements[elementIndex].textContent, tokens[currentToken].value);
            component->jsxElements[elementIndex].hasTextContent = true;
            currentToken++;
        } else if (match(TOKEN_OPEN_BRACE)) {
            // 入れ子になった中括弧の場合 - 式を処理
            consume(TOKEN_OPEN_BRACE);
            
            // 式の内容を収集
            int startPos = currentToken;
            int braceCount = 1;
            
            while (braceCount > 0 && !match(TOKEN_EOF)) {
                if (match(TOKEN_OPEN_BRACE)) {
                    braceCount++;
                } else if (match(TOKEN_CLOSE_BRACE)) {
                    braceCount--;
                }
                
                if (braceCount > 0) {
                    currentToken++;
                }
            }
            
            // 式の内容を文字列として保存
            char expression[MAX_TOKEN_LENGTH * 10] = "";
            for (int i = startPos; i < currentToken; i++) {
                strcat(expression, tokens[i].value);
                strcat(expression, " ");
            }
            
            // 子要素として式を追加
            int exprIndex = component->jsxElementCount;
            strcpy(component->jsxElements[exprIndex].tagName, "Expression");
            component->jsxElements[exprIndex].parentIndex = elementIndex;
            component->jsxElements[exprIndex].childCount = 0;
            component->jsxElements[exprIndex].attributeCount = 1;
            component->jsxElements[exprIndex].hasTextContent = false;
            
            // 式の値を属性として追加
            strcpy(component->jsxElements[exprIndex].attributes[0].name, "value");
            strcpy(component->jsxElements[exprIndex].attributes[0].value, expression);
            component->jsxElements[exprIndex].attributes[0].isExpression = true;
            
            component->jsxElements[elementIndex].childrenIndices[component->jsxElements[elementIndex].childCount++] = exprIndex;
            component->jsxElementCount++;
            
            expect(TOKEN_CLOSE_BRACE, "式の終了には '}' が必要です");
        } else {
            // 子要素がJSX要素の場合
            while (!match(TOKEN_CLOSE_BRACE) && !match(TOKEN_EOF)) {
                if (match(TOKEN_IDENTIFIER) || match(TOKEN_HTML_TAG)) {
                    int childIndex = parseJSXElement(component, elementIndex);
                    component->jsxElements[elementIndex].childrenIndices[component->jsxElements[elementIndex].childCount++] = childIndex;
                } else if (match(TOKEN_SHOW_WHEN)) {
                    // SHOW_WHEN ブロックを処理
                    parseShowWhen(component, elementIndex);
                } else if (match(TOKEN_FOR_EACH)) {
                    // FOR_EACH ブロックを処理
                    parseForEach(component, elementIndex);
                } else if (match(TOKEN_OPEN_BRACE)) {
                    // 式を処理
                    consume(TOKEN_OPEN_BRACE);
                    
                    // 式の内容を収集
                    int startPos = currentToken;
                    int braceCount = 1;
                    
                    while (braceCount > 0 && !match(TOKEN_EOF)) {
                        if (match(TOKEN_OPEN_BRACE)) {
                            braceCount++;
                        } else if (match(TOKEN_CLOSE_BRACE)) {
                            braceCount--;
                        }
                        
                        if (braceCount > 0) {
                            currentToken++;
                        }
                    }
                    
                    // 式の内容を文字列として保存
                    char expression[MAX_TOKEN_LENGTH * 10] = "";
                    for (int i = startPos; i < currentToken; i++) {
                        strcat(expression, tokens[i].value);
                        strcat(expression, " ");
                    }
                    
                    // 子要素として式を追加
                    int exprIndex = component->jsxElementCount;
                    strcpy(component->jsxElements[exprIndex].tagName, "Expression");
                    component->jsxElements[exprIndex].parentIndex = elementIndex;
                    component->jsxElements[exprIndex].childCount = 0;
                    component->jsxElements[exprIndex].attributeCount = 1;
                    component->jsxElements[exprIndex].hasTextContent = false;
                    
                    // 式の値を属性として追加
                    strcpy(component->jsxElements[exprIndex].attributes[0].name, "value");
                    strcpy(component->jsxElements[exprIndex].attributes[0].value, expression);
                    component->jsxElements[exprIndex].attributes[0].isExpression = true;
                    
                    component->jsxElements[elementIndex].childrenIndices[component->jsxElements[elementIndex].childCount++] = exprIndex;
                    component->jsxElementCount++;
                    
                    expect(TOKEN_CLOSE_BRACE, "式の終了には '}' が必要です");
                } else {
                    // その他のトークンはスキップ
                    currentToken++;
                }
            }
        }
        
        expect(TOKEN_CLOSE_BRACE, "JSX要素の終了には '}' が必要です");
    } else {
        // 子要素がない場合（自己終了タグ）
        // 何もしない
    }
    
    component->jsxElementCount++;
    return elementIndex;
}



/**
 * DISPLAY ブロックを解析
 */
void parseDisplay(Component* component) {
    // DISPLAY キーワードを消費
    expect(TOKEN_DISPLAY, "DISPLAY ブロックが必要です");
    
    // ブロック本体
    expect(TOKEN_OPEN_BRACE, "DISPLAY ブロックの開始には '{' が必要です");
    
    // ルートJSX要素を解析
    component->rootJSXElementIndex = parseJSXElement(component, -1);
    
    expect(TOKEN_CLOSE_BRACE, "DISPLAY ブロックの終了には '}' が必要です");
}

/**
 * スタイルルールを解析
 */
void parseStyleRule(Component* component) {
    // セレクタ
    char selector[MAX_TOKEN_LENGTH] = "";
    int selectorPos = 0;
    
    // セレクタの最初のトークンをチェック
    if (!match(TOKEN_IDENTIFIER) && !match(TOKEN_DOT) && !match(TOKEN_HASH)) {
        printError("スタイルセレクタが必要です", tokens[currentToken].line, tokens[currentToken].column);
    }
    
    // セレクタを収集
    while (!match(TOKEN_OPEN_BRACE) && !match(TOKEN_EOF)) {
        // トークンの値をセレクタに追加
        if (selectorPos + strlen(tokens[currentToken].value) < MAX_TOKEN_LENGTH - 1) {
            strcpy(selector + selectorPos, tokens[currentToken].value);
            selectorPos += strlen(tokens[currentToken].value);
        }
        
        currentToken++;
    }
    
    selector[selectorPos] = '\0';
    strcpy(component->styles[component->styleCount].selector, selector);
    
    // スタイルブロック
    expect(TOKEN_OPEN_BRACE, "スタイルブロックの開始には '{' が必要です");
    
    // スタイルプロパティを解析
    while (!match(TOKEN_CLOSE_BRACE) && !match(TOKEN_EOF)) {
        // プロパティ名
        if (match(TOKEN_IDENTIFIER)) {
            strcpy(component->styles[component->styleCount].property, tokens[currentToken].value);
            currentToken++;
            
            // コロンを期待
            expect(TOKEN_COLON, "スタイルプロパティには ':' が必要です");
            
            // プロパティ値
            char value[MAX_TOKEN_LENGTH] = "";
            int valuePos = 0;
            
            // 値を収集（セミコロンまで）
            while (!match(TOKEN_SEMICOLON) && !match(TOKEN_CLOSE_BRACE) && !match(TOKEN_EOF)) {
                if (valuePos + strlen(tokens[currentToken].value) < MAX_TOKEN_LENGTH - 1) {
                    strcpy(value + valuePos, tokens[currentToken].value);
                    valuePos += strlen(tokens[currentToken].value);
                }
                currentToken++;
            }
            
            value[valuePos] = '\0';
            strcpy(component->styles[component->styleCount].value, value);
            
            // セミコロンを期待
            expect(TOKEN_SEMICOLON, "スタイルプロパティの終了には ';' が必要です");
            
            component->styleCount++;
        } else {
            currentToken++;
        }
    }
    
    expect(TOKEN_CLOSE_BRACE, "スタイルブロックの終了には '}' が必要です");
}


/**
 * STYLES ブロックを解析
 */
void parseStyles(Component* component) {
    // STYLES キーワードを消費
    expect(TOKEN_STYLES, "STYLES ブロックが必要です");
    
    // ブロック本体
    expect(TOKEN_OPEN_BRACE, "STYLES ブロックの開始には '{' が必要です");
    
    // スタイルルールを解析
    while (!match(TOKEN_CLOSE_BRACE) && !match(TOKEN_EOF)) {
        parseStyleRule(component);
    }
    
    expect(TOKEN_CLOSE_BRACE, "STYLES ブロックの終了には '}' が必要です");
}

/**
 * コンポーネント定義を解析
 */
void parseComponent() {
    // COMPONENT キーワードを消費
    expect(TOKEN_COMPONENT, "コンポーネント定義が必要です");
    
    // コンポーネント名
    expect(TOKEN_IDENTIFIER, "コンポーネント名が必要です");
    strcpy(components[componentCount].name, tokens[currentToken - 1].value);
    
    // パラメータリスト
    expect(TOKEN_OPEN_PAREN, "パラメータリストの開始には '(' が必要です");
    
    // パラメータを解析（簡略化のため、生のテキストとして保存）
    char params[MAX_TOKEN_LENGTH] = "";
    while (!match(TOKEN_CLOSE_PAREN) && !match(TOKEN_EOF)) {
        strcat(params, tokens[currentToken].value);
        strcat(params, " ");
        currentToken++;
    }
    
    strcpy(components[componentCount].params, params);
    expect(TOKEN_CLOSE_PAREN, "パラメータリストの終了には ')' が必要です");
    
    // コンポーネント本体
    expect(TOKEN_OPEN_BRACE, "コンポーネント本体の開始には '{' が必要です");
    
    // 状態、アクション、表示などを解析
    while (!match(TOKEN_CLOSE_BRACE) && !match(TOKEN_EOF)) {
        if (match(TOKEN_STATE)) {
            parseState(&components[componentCount]);
        } else if (match(TOKEN_ASYNC) || match(TOKEN_ACTION)) {
            parseAction(&components[componentCount]);
        } else if (match(TOKEN_DISPLAY)) {
            parseDisplay(&components[componentCount]);
        } else if (match(TOKEN_DATA)) {
            parseData(&components[componentCount]);
        } else if (match(TOKEN_ON_LOAD)) {
            parseOnLoad(&components[componentCount]);
        } else if (match(TOKEN_STYLES)) {
            parseStyles(&components[componentCount]);
        } else {
            // 不明なトークンはスキップ
            currentToken++;
        }
    }
    
    expect(TOKEN_CLOSE_BRACE, "コンポーネント本体の終了には '}' が必要です");
    
    // エクスポート情報を初期化
    components[componentCount].isExported = false;
    
    componentCount++;
    printf("コンポーネント '%s' を解析しました\n", components[componentCount - 1].name);
}

/**
 * エクスポート文を解析
 */
void parseExport() {
    // EXPORT キーワードを消費
    expect(TOKEN_EXPORT, "エクスポート文が必要です");
    
    // エクスポートリスト
    expect(TOKEN_OPEN_PAREN, "エクスポートリストの開始には '(' が必要です");
    
    // エクスポートするコンポーネント名
    while (!match(TOKEN_CLOSE_PAREN) && !match(TOKEN_EOF)) {
        if (match(TOKEN_IDENTIFIER)) {
            // エクスポートするコンポーネントを探す
            for (int i = 0; i < componentCount; i++) {
                if (strcmp(components[i].name, tokens[currentToken].value) == 0) {
                    components[i].isExported = true;
                    printf("コンポーネント '%s' をエクスポートします\n", tokens[currentToken].value);
                    break;
                }
            }
            currentToken++;
        }
        
        // カンマをスキップ
        consume(TOKEN_COMMA);
    }
    
    expect(TOKEN_CLOSE_PAREN, "エクスポートリストの終了には ')' が必要です");
    expect(TOKEN_SEMICOLON, "エクスポート文の終了には ';' が必要です");
}

/**
 * 構文解析のメイン関数
 */
void parse() {
    currentToken = 0;
    componentCount = 0;
    
    while (!match(TOKEN_EOF)) {
        if (match(TOKEN_COMPONENT)) {
            parseComponent();
        } else if (match(TOKEN_EXPORT)) {
            parseExport();
        } else {
            // 不明なトークンはスキップ
            currentToken++;
        }
    }
    
    printf("構文解析完了: %d コンポーネントを解析しました\n", componentCount);
}

/**
 * HTMLタグ名をReactコンポーネント名に変換
 */
char* convertHtmlTagName(const char* tagName) {
    static char buffer[MAX_TOKEN_LENGTH];
    memset(buffer, 0, MAX_TOKEN_LENGTH); // バッファをクリア
    
    // 組み込みHTMLタグはそのまま返す
    if (strcmp(tagName, "div") == 0 || strcmp(tagName, "span") == 0 ||
        strcmp(tagName, "p") == 0 || strcmp(tagName, "h1") == 0 ||
        strcmp(tagName, "h2") == 0 || strcmp(tagName, "h3") == 0 ||
        strcmp(tagName, "button") == 0 || strcmp(tagName, "input") == 0 ||
        strcmp(tagName, "ul") == 0 || strcmp(tagName, "li") == 0 ||
        strcmp(tagName, "a") == 0 || strcmp(tagName, "img") == 0 ||
        strcmp(tagName, "form") == 0 || strcmp(tagName, "label") == 0 ||
        strcmp(tagName, "select") == 0 || strcmp(tagName, "option") == 0 ||
        strcmp(tagName, "textarea") == 0 || strcmp(tagName, "header") == 0 ||
        strcmp(tagName, "footer") == 0 || strcmp(tagName, "nav") == 0 ||
        strcmp(tagName, "section") == 0 || strcmp(tagName, "article") == 0 ||
        strcmp(tagName, "aside") == 0 || strcmp(tagName, "main") == 0) {
        strncpy(buffer, tagName, MAX_TOKEN_LENGTH - 1);
    } else {
        // カスタムコンポーネントは最初の文字を大文字に
        if (strlen(tagName) > 0) {
            buffer[0] = toupper(tagName[0]);
            strncpy(buffer + 1, tagName + 1, MAX_TOKEN_LENGTH - 2);
        }
    }
    
    return buffer;
}

/**
 * 出力バッファにテキストを追加
 */
void appendOutput(const char* text) {
    int len = strlen(text);
    if (outputPos + len < (int)OUTPUT_BUFFER_SIZE - 1) {
        strcpy(outputBuffer + outputPos, text);
        outputPos += len;
    }
}

/**
 * 出力バッファにフォーマット済みテキストを追加
 */
void appendOutputf(const char* format, ...) {
    char buffer[MAX_LINE_LENGTH];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    appendOutput(buffer);
}

/**
 * 出力ファイルに書き込む
 */
void writeOutput(const char* outputPath) {
    FILE* file = fopen(outputPath, "w");
    if (!file) {
        printf("エラー: 出力ファイル '%s' を開けませんでした\n", outputPath);
        return;
    }
    
    fprintf(file, "%s", outputBuffer);
    fclose(file);
    
    printf("出力ファイルに %d バイトを書き込みました\n", outputPos);
}

/**
 * 文字列複製関数
 */
char* myStrdup(const char* str) {
    size_t len = strlen(str) + 1;
    char* result = (char*)malloc(len);
    if (result == NULL) return NULL;
    return (char*)memcpy(result, str, len);
}

/**
 * メイン関数
 */
int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("使用法: %s <入力ファイル> <出力ファイル>\n", argv[0]);
        return 1;
    }
    
    const char* inputPath = argv[1];
    const char* outputPath = argv[2];
    
    // メモリ割り当て
    outputBuffer = (char*)malloc(OUTPUT_BUFFER_SIZE);
    if (!outputBuffer) {
        fprintf(stderr, "メモリ割り当てエラー: outputBuffer\n");
        return 1;
    }
    outputBuffer[0] = '\0';
    
    // グローバル変数の初期化
    outputPos = 0;
    tokenCount = 0;
    currentToken = 0;
    componentCount = 0;
    
    // トークン配列の割り当て
    tokens = (Token*)malloc(sizeof(Token) * MAX_TOKENS);
    if (!tokens) {
        fprintf(stderr, "メモリ割り当てエラー: tokens\n");
        free(outputBuffer);
        return 1;
    }
    
    // コンポーネント配列の割り当て - calloc を使用して0で初期化
    components = (Component*)calloc(MAX_COMPONENTS, sizeof(Component));
    if (!components) {
        fprintf(stderr, "メモリ割り当てエラー: components\n");
        free(outputBuffer);
        free(tokens);
        return 1;
    }

    // ソースファイル名を保存
    currentSourceFile = myStrdup(inputPath);
    if (currentSourceFile == NULL) {
        fprintf(stderr, "メモリ割り当てエラー: currentSourceFile\n");
        free(outputBuffer);
        free(tokens);
        free(components);
        return 1;
    }
    
    // ファイルを読み込む
    char* source = readFile(inputPath);
    if (!source) {
        free(outputBuffer);
        free(tokens);
        free(components);
        free(currentSourceFile);
        return 1;
    }
    
    // ソースコードの行を保存
    saveSourceLines(source);

    // トークン化
    tokenize(source);
    
    // 構文解析
    parse();
    
    // コード生成
    generateCode();
    
    // 出力ファイルに書き込む
    writeOutput(outputPath);
    
    printf("コンパイル完了: '%s' -> '%s'\n", inputPath, outputPath);
    return 0;

    // メモリ解放
    if (source) {
        free(source);
        source = NULL;
    }

    if (currentSourceFile) {
        free(currentSourceFile);
        currentSourceFile = NULL;
    }

    if (sourceLines) {
        for (int i = 0; i < sourceLineCount; i++) {
            if (sourceLines[i]) {
                free(sourceLines[i]);
                sourceLines[i] = NULL;
            }
        }
        free(sourceLines);
        sourceLines = NULL;
    }

    if (outputBuffer) {
        free(outputBuffer);
        outputBuffer = NULL;
    }

    if (tokens) {
        free(tokens);
        tokens = NULL;
    }

    if (components) {
        // コンポーネント内の動的に確保されたメモリを解放
        for (int i = 0; i < componentCount; i++) {
            // 必要に応じて、コンポーネント内の動的に確保されたメモリを解放
        }
        free(components);
        components = NULL;
    }

    printf("コンパイル完了: '%s' -> '%s'\n", inputPath, outputPath);
    return 0;
}
