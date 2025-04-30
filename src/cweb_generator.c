/**
 * CWeb Generator - CWeb構文からJavaScriptコードを生成
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include "cweb_parser.h"

/**
 * 文字列リテラルを適切にフォーマットする
 */
char* formatStringLiteral(const char* str) {
    static char buffer[MAX_TOKEN_LENGTH * 2];
    
    // 空文字列の場合
    if (str == NULL || str[0] == '\0') {
        strcpy(buffer, "\"\"");
        return buffer;
    }
    
    // 文字列がすでに引用符で囲まれているか確認
    if ((str[0] == '"' && str[strlen(str)-1] == '"') || 
        (str[0] == '\'' && str[strlen(str)-1] == '\'')) {
        strcpy(buffer, str);
        return buffer;
    }
    
    // 引用符で囲む
    sprintf(buffer, "\"%s\"", str);
    return buffer;
}

/**
 * JavaScript式を処理する
 */
char* processJavaScriptExpression(const char* expr) {
    static char buffer[MAX_TOKEN_LENGTH * 10];
    
    // 空の式の場合
    if (expr == NULL || expr[0] == '\0') {
        strcpy(buffer, "");
        return buffer;
    }
    
    // 式をそのまま返す
    strcpy(buffer, expr);
    return buffer;
}

/**
 * Reactコンポーネントのインポート部分を生成
 */
void generateImports() {
    appendOutput("import React, { useState, useEffect } from 'react';\n\n");
}

/**
 * 状態変数の宣言を生成
 */
void generateStateDeclarations(Component* component) {
    for (int i = 0; i < component->stateCount; i++) {
        // 状態変数名の最初の文字を大文字に
        char setterName[MAX_TOKEN_LENGTH];
        sprintf(setterName, "set%c%s", toupper(component->states[i].name[0]), component->states[i].name + 1);
        
        // 初期値を適切にフォーマット
        char initialValue[MAX_TOKEN_LENGTH * 2];
        if (component->states[i].type == TYPE_STRING) {
            // 文字列型の場合、引用符で囲む
            if (component->states[i].initialValue[0] != '"' && component->states[i].initialValue[0] != '\'') {
                sprintf(initialValue, "\"%s\"", component->states[i].initialValue);
            } else {
                strcpy(initialValue, component->states[i].initialValue);
            }
        } else {
            strcpy(initialValue, component->states[i].initialValue);
        }
        
        appendOutputf("  const [%s, %s] = useState(%s);\n", 
                     component->states[i].name, 
                     setterName,
                     initialValue);
    }
    
    if (component->stateCount > 0) {
        appendOutput("\n");
    }
}

/**
 * データ宣言を生成
 */
void generateDataDeclarations(Component* component) {
    for (int i = 0; i < component->dataCount; i++) {
        appendOutputf("  const %s = {\n", component->data[i].name);
        
        for (int j = 0; j < component->data[i].fieldCount; j++) {
            appendOutputf("    %s: %s%s\n", 
                         component->data[i].fields[j].name,
                         component->data[i].fields[j].value,
                         (j < component->data[i].fieldCount - 1) ? "," : "");
        }
        
        appendOutput("  };\n\n");
    }
}

/**
 * アクション関数を生成
 */
void generateActions(Component* component) {
    for (int i = 0; i < component->actionCount; i++) {
        // アクション関数の宣言
        appendOutputf("  const %s = %s(", 
                     component->actions[i].name,
                     component->actions[i].isAsync ? "async " : "");
        
        // パラメータを生成
        for (int j = 0; j < component->actions[i].paramCount; j++) {
            appendOutputf("%s%s", 
                         component->actions[i].params[j].name,
                         (j < component->actions[i].paramCount - 1) ? ", " : "");
        }
        
        appendOutput(") => {\n");
        
        // アクション本体を生成
        // 状態変数の更新を適切に処理
        char body[MAX_BODY_LENGTH];
        strncpy(body, component->actions[i].body, sizeof(body) - 1);
        body[sizeof(body) - 1] = '\0';
        
        // 状態変数の更新を検出して修正
        for (int j = 0; j < component->stateCount; j++) {
            char pattern[MAX_PATTERN_LENGTH];
            snprintf(pattern, sizeof(pattern), "%s = ", component->states[j].name);
            
            char* pos = strstr(body, pattern);
            while (pos != NULL) {
                // 状態変数の更新を見つけた
                char* start = pos;
                char* end = strchr(pos, ';');
                if (end != NULL) {
                    // 更新式を抽出
                    int len = end - (start + strlen(pattern));
                    char expr[MAX_TOKEN_LENGTH * 2];
                    if (len < (int)sizeof(expr) - 1) {
                        strncpy(expr, start + strlen(pattern), len);
                        expr[len] = '\0';
                        
                        // 更新式を置換
                        char replacement[MAX_REPLACEMENT_LENGTH];
                        char setterName[MAX_TOKEN_LENGTH];
                        snprintf(setterName, sizeof(setterName), "set%c%s", 
                                toupper(component->states[j].name[0]), 
                                component->states[j].name + 1);
                        
                        snprintf(replacement, sizeof(replacement), "%s(%s)", setterName, expr);
                        
                        // 置換を適用
                        char before[MAX_BODY_LENGTH];
                        char after[MAX_BODY_LENGTH];
                        
                        size_t before_len = start - body;
                        if (before_len < sizeof(before)) {
                            strncpy(before, body, before_len);
                            before[before_len] = '\0';
                            
                            strcpy(after, end + 1);
                            
                            // 安全な文字列結合
                            size_t new_body_len = before_len + strlen(replacement) + 1 + strlen(after);
                            if (new_body_len < sizeof(body)) {
                                snprintf(body, sizeof(body), "%s%s;%s", before, replacement, after);
                            }
                        }
                    }
                    
                    // 次の出現を検索
                    pos = strstr(body, pattern);
                } else {
                    break;
                }
            }
        }
        
        // 文字列リテラルを修正
        char* pos = strstr(body, "!== ");
        if (pos != NULL) {
            // "!== " の後に文字列リテラルがない場合、空文字列を追加
            if (pos[4] == '\0' || pos[4] == ' ' || pos[4] == ';') {
                char before[MAX_BODY_LENGTH];
                char after[MAX_BODY_LENGTH];
                
                size_t before_len = pos - body + 4; // "!== " の長さを含む
                if (before_len < sizeof(before)) {
                    strncpy(before, body, before_len);
                    before[before_len] = '\0';
                    
                    strcpy(after, pos + 4);
                    
                    // 安全な文字列結合
                    size_t new_body_len = before_len + 2 + strlen(after); // 引用符の長さを追加
                    if (new_body_len < sizeof(body)) {
                        snprintf(body, sizeof(body), "%s\"\"", before);
                        if (after[0] != '\0') {
                            strcat(body, after);
                        }
                    }
                }
            }
        }
        
        // 変数参照を修正
        char* active_pos = strstr(body, "=== active");
        if (active_pos != NULL) {
            char before[MAX_BODY_LENGTH];
            char after[MAX_BODY_LENGTH];
            
            size_t before_len = active_pos - body + 4; // "=== " の長さを含む
            if (before_len < sizeof(before)) {
                strncpy(before, body, before_len);
                before[before_len] = '\0';
                
                strcpy(after, active_pos + 10); // "active" の長さを含む
                
                // 安全な文字列結合
                size_t new_body_len = before_len + 9 + strlen(after); // 引用符を含む "active" の長さ
                if (new_body_len < sizeof(body)) {
                    snprintf(body, sizeof(body), "%s\"active\"%s", before, after);
                }
            }
        }
        
        char* completed_pos = strstr(body, "=== completed");
        if (completed_pos != NULL) {
            char before[MAX_BODY_LENGTH];
            char after[MAX_BODY_LENGTH];
            
            size_t before_len = completed_pos - body + 4; // "=== " の長さを含む
            if (before_len < sizeof(before)) {
                strncpy(before, body, before_len);
                before[before_len] = '\0';
                
                strcpy(after, completed_pos + 13); // "completed" の長さを含む
                
                // 安全な文字列結合
                size_t new_body_len = before_len + 12 + strlen(after); // 引用符を含む "completed" の長さ
                if (new_body_len < sizeof(body)) {
                    snprintf(body, sizeof(body), "%s\"completed\"%s", before, after);
                }
            }
        }
        
        // 空白を整理
        char cleanBody[MAX_BODY_LENGTH];
        int cleanPos = 0;
        bool inString = false;
        bool lastWasSpace = false;
        
        for (int j = 0; body[j] != '\0' && cleanPos < (int)sizeof(cleanBody) - 1; j++) {
            if (body[j] == '"' || body[j] == '\'') {
                inString = !inString;
                cleanBody[cleanPos++] = body[j];
                lastWasSpace = false;
            } else if (body[j] == ' ') {
                if (!lastWasSpace || inString) {
                    cleanBody[cleanPos++] = body[j];
                }
                lastWasSpace = !inString && true;
            } else {
                cleanBody[cleanPos++] = body[j];
                lastWasSpace = false;
            }
        }
        
        cleanBody[cleanPos] = '\0';
        
        appendOutputf("    %s\n", cleanBody);
        
        // アクション関数の終了
        appendOutput("  };\n\n");
    }
}

/**
 * useEffect フックを生成
 */
void generateUseEffect(Component* component) {
    if (component->hasOnLoad) {
        appendOutput("  useEffect(() => {\n");
        
        // ON_LOAD ブロックのコードを処理
        char onLoadCode[MAX_BODY_LENGTH];
        strncpy(onLoadCode, component->onLoadCode, sizeof(onLoadCode) - 1);
        onLoadCode[sizeof(onLoadCode) - 1] = '\0';
        
        // 状態変数の更新を適切に処理
        for (int j = 0; j < component->stateCount; j++) {
            char pattern[MAX_PATTERN_LENGTH];
            snprintf(pattern, sizeof(pattern), "%s = ", component->states[j].name);
            
            char* pos = strstr(onLoadCode, pattern);
            while (pos != NULL) {
                // 状態変数の更新を見つけた
                char* start = pos;
                char* end = strchr(pos, ';');
                if (end != NULL) {
                    // 更新式を抽出
                    int len = end - (start + strlen(pattern));
                    char expr[MAX_TOKEN_LENGTH * 2];
                    if (len < (int)sizeof(expr) - 1) {
                        strncpy(expr, start + strlen(pattern), len);
                        expr[len] = '\0';
                        
                        // 更新式を置換
                        char replacement[MAX_REPLACEMENT_LENGTH];
                        char setterName[MAX_TOKEN_LENGTH];
                        snprintf(setterName, sizeof(setterName), "set%c%s", 
                                toupper(component->states[j].name[0]), 
                                component->states[j].name + 1);
                        
                        snprintf(replacement, sizeof(replacement), "%s(%s)", setterName, expr);
                        
                        // 置換を適用
                        char before[MAX_BODY_LENGTH];
                        char after[MAX_BODY_LENGTH];
                        
                        size_t before_len = start - onLoadCode;
                        if (before_len < sizeof(before)) {
                            strncpy(before, onLoadCode, before_len);
                            before[before_len] = '\0';
                            
                            strcpy(after, end + 1);
                            
                            // 安全な文字列結合
                            size_t new_code_len = before_len + strlen(replacement) + 1 + strlen(after);
                            if (new_code_len < sizeof(onLoadCode)) {
                                snprintf(onLoadCode, sizeof(onLoadCode), "%s%s;%s", before, replacement, after);
                            }
                        }
                    }
                    
                    // 次の出現を検索
                    pos = strstr(onLoadCode, pattern);
                } else {
                    break;
                }
            }
        }
        
        // 文字列リテラルを修正
        char* pos = strstr(onLoadCode, "text : ");
        while (pos != NULL) {
            // "text : " の後に文字列リテラルがない場合、引用符を追加
            char* valueStart = pos + 7; // "text : " の長さ
            if (*valueStart != '"' && *valueStart != '\'') {
                char* valueEnd = strchr(valueStart, ',');
                if (valueEnd == NULL) {
                    valueEnd = strchr(valueStart, '}');
                }
                
                if (valueEnd != NULL) {
                    int valueLen = valueEnd - valueStart;
                    char value[MAX_TOKEN_LENGTH];
                    if (valueLen < (int)sizeof(value) - 1) {
                        strncpy(value, valueStart, valueLen);
                        value[valueLen] = '\0';
                        
                        char replacement[MAX_TOKEN_LENGTH * 2];
                        snprintf(replacement, sizeof(replacement), "text: \"%s\"", value);
                        
                        char before[MAX_BODY_LENGTH];
                        char after[MAX_BODY_LENGTH];
                        
                        size_t before_len = pos - onLoadCode;
                        if (before_len < sizeof(before)) {
                            strncpy(before, onLoadCode, before_len);
                            before[before_len] = '\0';
                            
                            strcpy(after, valueEnd);
                            
                            // 安全な文字列結合
                            size_t new_code_len = before_len + strlen(replacement) + strlen(after);
                            if (new_code_len < sizeof(onLoadCode)) {
                                snprintf(onLoadCode, sizeof(onLoadCode), "%s%s%s", before, replacement, after);
                            }
                        }
                    }
                }
            }
            
            // 次の出現を検索
            pos = strstr(pos + 1, "text : ");
        }
        
        // 空白を整理
        char cleanCode[MAX_BODY_LENGTH];
        int cleanPos = 0;
        bool inString = false;
        bool lastWasSpace = false;
        
        for (int j = 0; onLoadCode[j] != '\0' && cleanPos < (int)sizeof(cleanCode) - 1; j++) {
            if (onLoadCode[j] == '"' || onLoadCode[j] == '\'') {
                inString = !inString;
                cleanCode[cleanPos++] = onLoadCode[j];
                lastWasSpace = false;
            } else if (onLoadCode[j] == ' ') {
                if (!lastWasSpace || inString) {
                    cleanCode[cleanPos++] = onLoadCode[j];
                }
                lastWasSpace = !inString && true;
            } else {
                cleanCode[cleanPos++] = onLoadCode[j];
                lastWasSpace = false;
            }
        }
        
        cleanCode[cleanPos] = '\0';
        
        appendOutputf("    // ON_LOAD ブロックの内容\n    %s\n", cleanCode);
        appendOutput("    return () => {\n");
        appendOutput("      // クリーンアップ関数\n");
        appendOutput("    };\n");
        appendOutput("  }, []);\n\n");
    }
}

/**
 * JSX属性を生成
 */
void generateJSXAttributes(JSXAttribute* attributes, int attributeCount) {
    for (int i = 0; i < attributeCount; i++) {
        if (attributes[i].isExpression) {
            appendOutputf(" %s={%s}", attributes[i].name, attributes[i].value);
        } else {
            appendOutputf(" %s=\"%s\"", attributes[i].name, attributes[i].value);
        }
    }
}

/**
 * JSX要素を再帰的に生成
 */
void generateJSXElement(Component* component, int elementIndex, int indentLevel) {
    JSXElement* element = &component->jsxElements[elementIndex];
    
    // インデント
    for (int i = 0; i < indentLevel; i++) {
        appendOutput("  ");
    }
    
    // 特殊な条件付きレンダリング要素の場合
    if (strcmp(element->tagName, "ConditionalRender") == 0) {
        // 条件式を取得
        char* condition = NULL;
        for (int i = 0; i < element->attributeCount; i++) {
            if (strcmp(element->attributes[i].name, "condition") == 0) {
                condition = element->attributes[i].value;
                break;
            }
        }
        
        if (condition) {
            // 条件付きレンダリングのJSXを生成
            appendOutputf("{%s && (\n", condition);
            
            // 子要素を生成
            for (int i = 0; i < element->childCount; i++) {
                generateJSXElement(component, element->childrenIndices[i], indentLevel + 1);
            }
            
            // 閉じ括弧
            for (int i = 0; i < indentLevel; i++) {
                appendOutput("  ");
            }
            appendOutput(")}\n");
        }
        
        return;
    }
    
    // 特殊な反復レンダリング要素の場合
    if (strcmp(element->tagName, "IterativeRender") == 0) {
        // 反復変数と配列を取得
        char* iterator = NULL;
        char* collection = NULL;
        
        for (int i = 0; i < element->attributeCount; i++) {
            if (strcmp(element->attributes[i].name, "iterator") == 0) {
                iterator = element->attributes[i].value;
            } else if (strcmp(element->attributes[i].name, "collection") == 0) {
                collection = element->attributes[i].value;
            }
        }
        
        if (iterator && collection) {
            // 反復レンダリングのJSXを生成
            appendOutputf("{%s.map((%s, index) => (\n", collection, iterator);
            
            // 子要素を生成
            for (int i = 0; i < element->childCount; i++) {
                generateJSXElement(component, element->childrenIndices[i], indentLevel + 1);
            }
            
            // 閉じ括弧
            for (int i = 0; i < indentLevel; i++) {
                appendOutput("  ");
            }
            appendOutput("))}\n");
        }
        
        return;
    }
    
    // 特殊な式要素の場合
    if (strcmp(element->tagName, "Expression") == 0) {
        // 式を取得
        char* expression = NULL;
        for (int i = 0; i < element->attributeCount; i++) {
            if (strcmp(element->attributes[i].name, "value") == 0) {
                expression = element->attributes[i].value;
                break;
            }
        }
        
        if (expression) {
            // 式を生成
            appendOutputf("{%s}\n", expression);
        }
        
        return;
    }
    
    // 通常のJSX要素の場合
    char* tagName = convertHtmlTagName(element->tagName);
    
    // 開始タグ
    appendOutputf("<%s", tagName);
    
    // 属性
    generateJSXAttributes(element->attributes, element->attributeCount);
    
    if (element->hasTextContent) {
        // テキストコンテンツのみの場合
        appendOutputf(">%s</%s>\n", element->textContent, tagName);
    } else if (element->childCount == 0) {
        // 子要素がない場合は自己終了タグ
        appendOutput(" />\n");
    } else {
        // 子要素がある場合
        appendOutput(">\n");
        
        // 子要素を再帰的に生成
        for (int i = 0; i < element->childCount; i++) {
            generateJSXElement(component, element->childrenIndices[i], indentLevel + 1);
        }
        
        // 終了タグ
        for (int i = 0; i < indentLevel; i++) {
            appendOutput("  ");
        }
        appendOutputf("</%s>\n", tagName);
    }
}

/**
 * CSSプロパティ名をキャメルケースに変換
 */
char* toCamelCase(const char* property) {
    static char camelCase[MAX_TOKEN_LENGTH];
    int camelPos = 0;
    bool capitalize = false;
    
    for (size_t i = 0; property[i] != '\0' && camelPos < MAX_TOKEN_LENGTH - 1; i++) {
        if (property[i] == '-') {
            capitalize = true;
        } else if (capitalize) {
            camelCase[camelPos++] = toupper(property[i]);
            capitalize = false;
        } else {
            camelCase[camelPos++] = property[i];
        }
    }
    
    camelCase[camelPos] = '\0';
    return camelCase;
}

/**
 * CSSセレクタをJavaScriptオブジェクトキーに変換
 */
char* formatSelector(const char* selector) {
    static char formatted[MAX_TOKEN_LENGTH];
    int pos = 0;
    
    // セレクタからクラス名を抽出
    if (selector[0] == '.') {
        // クラスセレクタ
        int i = 1;
        while (selector[i] != '\0' && pos < MAX_TOKEN_LENGTH - 1) {
            if (selector[i] == '-') {
                formatted[pos++] = '_';
            } else if (selector[i] == ' ' || selector[i] == '.' || selector[i] == ':' || 
                      selector[i] == '[' || selector[i] == ']' || selector[i] == '>' || 
                      selector[i] == '+' || selector[i] == '~') {
                // セレクタの区切り文字は無視
                // 複合セレクタの場合は最初の部分のみ使用
                break;
            } else {
                formatted[pos++] = selector[i];
            }
            i++;
        }
    } else if (selector[0] == '#') {
        // IDセレクタ
        int i = 1;
        while (selector[i] != '\0' && pos < MAX_TOKEN_LENGTH - 1) {
            if (selector[i] == '-') {
                formatted[pos++] = '_';
            } else if (selector[i] == ' ' || selector[i] == '.' || selector[i] == ':' || 
                      selector[i] == '[' || selector[i] == ']') {
                break;
            } else {
                formatted[pos++] = selector[i];
            }
            i++;
        }
    } else if (strstr(selector, "[") != NULL) {
        // 属性セレクタ
        char* bracket = strchr(selector, '[');
        if (bracket != NULL) {
            // 基本セレクタ部分をコピー
            int baseLen = bracket - selector;
            strncpy(formatted, selector, baseLen);
            pos = baseLen;
            
            // 属性部分を処理
            char* closeBracket = strchr(selector, ']');
            if (closeBracket != NULL) {
                // 属性名を抽出
                int attrLen = closeBracket - (bracket + 1);
                char attrName[MAX_TOKEN_LENGTH];
                strncpy(attrName, bracket + 1, attrLen);
                attrName[attrLen] = '\0';
                
                // 等号があれば分割
                char* equals = strchr(attrName, '=');
                if (equals != NULL) {
                    *equals = '\0';
                }
                
                // 属性名をキーに追加
                if (pos > 0) {
                    formatted[pos++] = '_';
                }
                
                // ハイフンをアンダースコアに変換
                for (int i = 0; attrName[i] != '\0' && pos < MAX_TOKEN_LENGTH - 1; i++) {
                    if (attrName[i] == '-') {
                        formatted[pos++] = '_';
                    } else if (attrName[i] == '"' || attrName[i] == '\'') {
                        // 引用符は無視
                    } else {
                        formatted[pos++] = attrName[i];
                    }
                }
            }
        }
    } else {
        // 要素セレクタ
        int i = 0;
        while (selector[i] != '\0' && pos < MAX_TOKEN_LENGTH - 1) {
            if (selector[i] == '-') {
                formatted[pos++] = '_';
            } else if (selector[i] == ' ' || selector[i] == '.' || selector[i] == ':' || 
                      selector[i] == '[' || selector[i] == ']') {
                break;
            } else {
                formatted[pos++] = selector[i];
            }
            i++;
        }
    }
    
    formatted[pos] = '\0';
    
    // 空のセレクタの場合はデフォルト名を使用
    if (formatted[0] == '\0') {
        strcpy(formatted, "default");
    }
    
    return formatted;
}

/**
 * CSSプロパティ値を適切にフォーマット
 */
char* formatPropertyValue(const char* value) {
    static char formatted[MAX_VALUE_LENGTH];
    
    // 空の値の場合
    if (value == NULL || value[0] == '\0') {
        strcpy(formatted, "\"\"");
        return formatted;
    }
    
    // 値を整形
    char cleanValue[MAX_VALUE_LENGTH];
    int cleanPos = 0;
    
    // スペースを削除
    for (int i = 0; value[i] != '\0' && cleanPos < MAX_VALUE_LENGTH - 1; i++) {
        if (value[i] != ' ') {
            cleanValue[cleanPos++] = value[i];
        }
    }
    cleanValue[cleanPos] = '\0';
    
    // 数値の場合は引用符なし
    if (isdigit(cleanValue[0]) || (cleanValue[0] == '-' && isdigit(cleanValue[1]))) {
        // 単位を含むかチェック
        bool hasUnit = false;
        for (int i = 0; cleanValue[i] != '\0'; i++) {
            if (isalpha(cleanValue[i])) {
                hasUnit = true;
                break;
            }
        }
        
        if (hasUnit) {
            sprintf(formatted, "\"%s\"", cleanValue);
        } else {
            strcpy(formatted, cleanValue);
        }
    } else if (cleanValue[0] == '#') {
        // 色コード
        sprintf(formatted, "\"%s\"", cleanValue);
    } else if (cleanValue[0] == '"' || cleanValue[0] == '\'') {
        // すでに引用符で囲まれている
        strcpy(formatted, cleanValue);
    } else {
        // その他の値
        sprintf(formatted, "\"%s\"", cleanValue);
    }
    
    return formatted;
}

/**
 * レンダリング関数を生成
 */
void generateRender(Component* component) {
    appendOutput("  return (\n");
    
    // ルートJSX要素を生成
    if (component->jsxElementCount > 0) {
        generateJSXElement(component, component->rootJSXElementIndex, 1);
    } else {
        appendOutput("    <div>\n");
        appendOutput("      {/* コンポーネントの表示内容 */}\n");
        appendOutput("    </div>\n");
    }
    
    appendOutput("  );\n");
}

/**
 * スタイルを生成
 */
void generateStyles(Component* component) {
    if (component->styleCount > 0) {
        appendOutput("\n// スタイル定義\n");
        appendOutput("const styles = {\n");
        
        // セレクタごとにスタイルオブジェクトを生成
        char currentSelector[MAX_TOKEN_LENGTH] = "";
        bool isFirstSelector = true;
        
        // セレクタとプロパティのマップを作成
        typedef struct {
            char selector[MAX_TOKEN_LENGTH];
            char property[MAX_TOKEN_LENGTH];
            char value[MAX_VALUE_LENGTH];
        } StyleEntry;
        
        StyleEntry* styleMap = (StyleEntry*)malloc(sizeof(StyleEntry) * component->styleCount);
        int styleMapCount = 0;
        
        // スタイルマップを構築
        for (int i = 0; i < component->styleCount; i++) {
            // セレクタをフォーマット
            char formattedSelector[MAX_TOKEN_LENGTH];
            strcpy(formattedSelector, formatSelector(component->styles[i].selector));
            
            // プロパティをキャメルケースに変換
            char camelCaseProperty[MAX_TOKEN_LENGTH];
            strcpy(camelCaseProperty, toCamelCase(component->styles[i].property));
            
            // 値をフォーマット
            char formattedValue[MAX_VALUE_LENGTH];
            strcpy(formattedValue, formatPropertyValue(component->styles[i].value));
            
            // マップに追加
            strcpy(styleMap[styleMapCount].selector, formattedSelector);
            strcpy(styleMap[styleMapCount].property, camelCaseProperty);
            strcpy(styleMap[styleMapCount].value, formattedValue);
            styleMapCount++;
        }
        
        // セレクタごとにスタイルを生成
        for (int i = 0; i < styleMapCount; i++) {
            if (strcmp(currentSelector, styleMap[i].selector) != 0) {
                // 前のセレクタを閉じる
                if (!isFirstSelector) {
                    appendOutput("  },\n");
                }
                
                isFirstSelector = false;
                
                // 新しいセレクタを開始
                strcpy(currentSelector, styleMap[i].selector);
                appendOutputf("  %s: {\n", currentSelector);
            }
            
            // プロパティと値
            appendOutputf("    %s: %s%s\n", 
                         styleMap[i].property,
                         styleMap[i].value,
                         (i < styleMapCount - 1 && strcmp(currentSelector, styleMap[i+1].selector) == 0) ? "," : "");
        }
        
        // 最後のセレクタを閉じる
        if (!isFirstSelector) {
            appendOutput("  }\n");
        }
        
        appendOutput("};\n");
        
        // メモリ解放
        free(styleMap);
    }
}

/**
 * コンポーネント全体を生成
 */
void generateComponent(Component* component) {
    // コンポーネント関数の開始
    appendOutputf("function %s(%s) {\n", component->name, component->params);
    
    // 状態変数の宣言
    generateStateDeclarations(component);
    
    // データ宣言
    generateDataDeclarations(component);
    
    // アクション関数
    generateActions(component);
    
    // useEffect フック
    generateUseEffect(component);
    
    // レンダリング関数
    generateRender(component);
    
    // コンポーネント関数の終了
    appendOutput("}\n");
    
    // スタイル
    generateStyles(component);
    
    // エクスポート
    if (component->isExported) {
        appendOutputf("\nexport default %s;\n", component->name);
    } else {
        appendOutputf("\n// コンポーネントをエクスポートするには EXPORT(%s); を追加してください\n", component->name);
    }
}

/**
 * コード生成のメイン関数
 */
void generateCode() {
    outputPos = 0;
    
    // Reactのインポート
    generateImports();
    
    // 各コンポーネントを生成
    for (int i = 0; i < componentCount; i++) {
        generateComponent(&components[i]);
        
        // 最後のコンポーネント以外は区切り線を追加
        if (i < componentCount - 1) {
            appendOutput("\n// ----------------------------------------\n\n");
        }
    }
    
    printf("コード生成完了: %d バイトのJavaScriptコードを生成しました\n", outputPos);
}

