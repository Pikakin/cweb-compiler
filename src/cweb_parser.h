/**
 * CWeb Parser - CWeb構文解析のためのヘッダーファイル
 */

#ifndef CWEB_PARSER_H
#define CWEB_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>

// 定数定義
#define MAX_TOKEN_LENGTH 1024
#define MAX_LINE_LENGTH 4096
#define MAX_TOKENS 3000
#define MAX_COMPONENTS 30
#define MAX_STATES 50
#define MAX_ACTIONS 50
#define MAX_NESTED_LEVEL 50
#define OUTPUT_BUFFER_SIZE (MAX_LINE_LENGTH * 1000)
#define _POSIX_C_SOURCE 200809L  // strdup用
#define MAX_PATTERN_LENGTH 2048
#define MAX_REPLACEMENT_LENGTH 4096
#define MAX_BODY_LENGTH (MAX_TOKEN_LENGTH * 100)
#define MAX_VALUE_LENGTH 2048


// トークンの種類
typedef enum {
    TOKEN_COMPONENT,
    TOKEN_NOT,           // !
    TOKEN_NOT_EQUALS,    // !=
    TOKEN_STRICT_NOT_EQUALS, // !==
    TOKEN_EQUALS_EQUALS, // ==
    TOKEN_STRICT_EQUALS, // ===
    TOKEN_STATE,
    TOKEN_ACTION,
    TOKEN_DISPLAY,
    TOKEN_DATA,
    TOKEN_ASYNC,
    TOKEN_AWAIT,
    TOKEN_TRY,
    TOKEN_CATCH,
    TOKEN_FINALLY,
    TOKEN_ON_LOAD,
    TOKEN_SHOW_WHEN,
    TOKEN_FOR_EACH,
    TOKEN_STYLES,
    TOKEN_EXPORT,
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_EQUALS,
    TOKEN_PLUS,
    TOKEN_PLUS_EQUALS,   // +=
    TOKEN_PLUS_PLUS,     // ++
    TOKEN_MINUS,
    TOKEN_MINUS_EQUALS,  // -=
    TOKEN_MINUS_MINUS,   // --
    TOKEN_MULTIPLY,
    TOKEN_MULTIPLY_EQUALS, // *=
    TOKEN_DIVIDE,
    TOKEN_DIVIDE_EQUALS, // /=
    TOKEN_MODULO,        // %
    TOKEN_MODULO_EQUALS, // %=
    TOKEN_BITWISE_AND,   // &
    TOKEN_AND,           // &&
    TOKEN_BITWISE_OR,    // |
    TOKEN_OR,            // ||
    TOKEN_BITWISE_XOR,   // ^
    TOKEN_BITWISE_NOT,   // ~
    TOKEN_SHIFT_LEFT,    // <<
    TOKEN_SHIFT_RIGHT,   // >>
    TOKEN_GREATER_THAN,          // >
    TOKEN_GREATER_THAN_EQUALS,   // >=
    TOKEN_HASH,                // #
    TOKEN_UNSIGNED_SHIFT_RIGHT, // >>>
    TOKEN_QUESTION,      // ?
    TOKEN_NULLISH_COALESCING, // ??
    TOKEN_OPTIONAL_CHAINING, // ?.
    TOKEN_SPREAD,        // ...
    TOKEN_OPEN_PAREN,
    TOKEN_CLOSE_PAREN,
    TOKEN_OPEN_BRACE,
    TOKEN_CLOSE_BRACE,
    TOKEN_OPEN_BRACKET,
    TOKEN_CLOSE_BRACKET,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_DOT,
    TOKEN_ARROW,
    TOKEN_DOUBLE_ARROW,  
    TOKEN_HTML_TAG,
    TOKEN_COLON, 
    TOKEN_EOF
} TokenType;

// トークン構造体
typedef struct {
    TokenType type;
    char value[MAX_TOKEN_LENGTH];
    int line;
    int column;
} Token;

// 状態変数の型
typedef enum {
    TYPE_STRING,
    TYPE_NUMBER,
    TYPE_BOOLEAN,
    TYPE_ARRAY,
    TYPE_OBJECT,
    TYPE_ANY
} StateType;

// 状態変数
typedef struct {
    char name[MAX_TOKEN_LENGTH];
    StateType type;
    char initialValue[MAX_TOKEN_LENGTH];
} State;

// データフィールド
typedef struct {
    char name[MAX_TOKEN_LENGTH];
    char value[MAX_TOKEN_LENGTH];
} DataField;

// データ宣言
typedef struct {
    char name[MAX_TOKEN_LENGTH];
    DataField fields[MAX_STATES];
    int fieldCount;
} Data;

// アクションパラメータ
typedef struct {
    char name[MAX_TOKEN_LENGTH];
    char type[MAX_TOKEN_LENGTH];
} ActionParam;

// アクション関数
typedef struct {
    char name[MAX_TOKEN_LENGTH];
    bool isAsync;
    ActionParam params[MAX_STATES];
    int paramCount;
    char body[MAX_TOKEN_LENGTH * 50];  
} Action;

// JSX属性
typedef struct {
    char name[MAX_TOKEN_LENGTH];
    char value[MAX_TOKEN_LENGTH * 10];
    bool isExpression;
} JSXAttribute;

// JSX要素
typedef struct JSXElement {
    char tagName[MAX_TOKEN_LENGTH];
    JSXAttribute attributes[MAX_STATES];
    int attributeCount;
    bool hasTextContent;
    char textContent[MAX_TOKEN_LENGTH * 10]; 
    int childrenIndices[MAX_NESTED_LEVEL];
    int childCount;
    int parentIndex;
} JSXElement;

// スタイル
typedef struct {
    char selector[MAX_TOKEN_LENGTH];
    char property[MAX_TOKEN_LENGTH];
    char value[MAX_TOKEN_LENGTH];
} Style;

// コンポーネント情報
typedef struct {
    char name[MAX_TOKEN_LENGTH];
    char params[MAX_TOKEN_LENGTH];
    State states[MAX_STATES];
    int stateCount;
    Action actions[MAX_ACTIONS];
    int actionCount;
    Data data[MAX_STATES];
    int dataCount;
    bool hasOnLoad;
    char onLoadCode[MAX_TOKEN_LENGTH * 50];
    JSXElement jsxElements[MAX_NESTED_LEVEL * 10];
    int jsxElementCount;
    int rootJSXElementIndex;
    Style styles[MAX_STATES];
    int styleCount;
    bool isExported;
} Component;

// 外部変数宣言
extern char* outputBuffer;
extern int outputPos;
extern Token* tokens;
extern int tokenCount;
extern int currentToken;
extern Component* components;
extern int componentCount;
extern char* currentSourceFile;
extern char** sourceLines;
extern int sourceLineCount;

// 関数プロトタイプ
char* readFile(const char* filePath);
void tokenize(const char* source);
void parse();
void generateCode();
void writeOutput(const char* outputPath);
void printError(const char* message, int line, int column);
void appendOutput(const char* text);
void appendOutputf(const char* format, ...);
bool match(TokenType type);
bool consume(TokenType type);
bool expect(TokenType type, const char* message);
void parseComponent();
void parseExport();
char* convertHtmlTagName(const char* tagName);
StateType parseStateType(const char* typeStr);
char* getStateTypeString(StateType type);
void parseState(Component* component);
void parseData(Component* component);
void parseAction(Component* component);
void parseOnLoad(Component* component);
void parseJSXAttributes(JSXElement* element);
int parseJSXElement(Component* component, int parentIndex);
void parseDisplay(Component* component);
void parseStyleRule(Component* component);
void parseStyles(Component* component);
const char* getTokenTypeName(TokenType type);
void parseShowWhen(Component* component, int parentIndex);
void parseForEach(Component* component, int parentIndex);
void saveSourceLines(const char* source);
char* formatStringLiteral(const char* str);
char* processJavaScriptExpression(const char* expr);

#endif /* CWEB_PARSER_H */
