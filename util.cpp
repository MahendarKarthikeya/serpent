#include <stdio.h>
#include <iostream>
#include <vector>
#include <map>
#include "util.h"
#include "bignum.h"
#include <fstream>
#include <cerrno>

//Token or value node constructor
Node token(std::string val, Metadata met) {
    Node o;
    o.type = 0;
    o.val = val;
    o.metadata = met;
    return o;
}

//AST node constructor
Node astnode(std::string val, std::vector<Node> args, Metadata met) {
    Node o;
    o.type = 1;
    o.val = val;
    o.args = args;
    o.metadata = met;
    return o;
}

// Print token list
std::string printTokens(std::vector<Node> tokens) {
    std::string s = "";
    for (int i = 0; i < tokens.size(); i++) {
        s += tokens[i].val + " ";
    }
    return s;
}

// Prints a lisp AST on one line
std::string printSimple(Node ast) {
    if (ast.type == TOKEN) return ast.val;
    std::string o = "(" + ast.val;
    std::vector<std::string> subs;
    for (int i = 0; i < ast.args.size(); i++) {
        o += " " + printSimple(ast.args[i]);
    }
    return o + ")";
}

// Number of tokens in a tree
int treeSize(Node prog) {
    if (prog.type == TOKEN) return 1;
    int o = 0;
    for (int i = 0; i < prog.args.size(); i++) o += treeSize(prog.args[i]);
    return o;
}

// Pretty-prints a lisp AST
std::string printAST(Node ast, bool printMetadata) {
    if (ast.type == TOKEN) return ast.val;
    std::string o = "(";
    if (printMetadata) {
         o += ast.metadata.file + " ";
         o += intToDecimal(ast.metadata.ln) + " ";
         o += intToDecimal(ast.metadata.ch) + ": ";
    }
    o += ast.val;
    std::vector<std::string> subs;
    for (int i = 0; i < ast.args.size(); i++) {
        subs.push_back(printAST(ast.args[i], printMetadata));
    }
    int k = 0;
    std::string out = " ";
    // As many arguments as possible go on the same line as the function,
    // except when seq is used
    while (k < subs.size() && o != "(seq") {
        if (subs[k].find("\n") != -1 || (out + subs[k]).length() >= 80) break;
        out += subs[k] + " ";
        k += 1;
    }
    // All remaining arguments go on their own lines
    if (k < subs.size()) {
        o += out + "\n";
        std::vector<std::string> subsSliceK;
        for (int i = k; i < subs.size(); i++) subsSliceK.push_back(subs[i]);
        o += indentLines(joinLines(subsSliceK));
        o += "\n)";
    }
    else {
        o += out.substr(0, out.size() - 1) + ")";
    }
    return o;
}

// Splits text by line
std::vector<std::string> splitLines(std::string s) {
    int pos = 0;
    int lastNewline = 0;
    std::vector<std::string> o;
    while (pos < s.length()) {
        if (s[pos] == '\n') {
            o.push_back(s.substr(lastNewline, pos - lastNewline));
            lastNewline = pos + 1;
        }
        pos = pos + 1;
    }
    o.push_back(s.substr(lastNewline));
    return o;
}

// Inverse of splitLines
std::string joinLines(std::vector<std::string> lines) {
    std::string o = "\n";
    for (int i = 0; i < lines.size(); i++) {
        o += lines[i] + "\n";
    }
    return o.substr(1, o.length() - 2);
}

// Indent all lines by 4 spaces
std::string indentLines(std::string inp) {
    std::vector<std::string> lines = splitLines(inp);
    for (int i = 0; i < lines.size(); i++) lines[i] = "    "+lines[i];
    return joinLines(lines);
}

// Converts string to simple numeric format
std::string strToNumeric(std::string inp) {
    std::string o = "0";
    if (inp == "") {
        o = "";
    }
    else if ((inp[0] == '"' && inp[inp.length()-1] == '"')
            || (inp[0] == '\'' && inp[inp.length()-1] == '\'')) {
        for (int i = 1; i < inp.length() - 1; i++) {
            o = decimalAdd(decimalMul(o,"256"), intToDecimal(inp[i]));
        }
    }
    else if (inp.substr(0,2) == "0x") {
        for (int i = 2; i < inp.length(); i++) {
            int dig = std::string("0123456789abcdef").find(inp[i]);
            if (dig < 0) return "";
            o = decimalAdd(decimalMul(o,"16"), intToDecimal(dig));
        }
    }
    else {
        bool isPureNum = true;
        for (int i = 0; i < inp.length(); i++) {
            isPureNum = isPureNum && inp[i] >= '0' && inp[i] <= '9';
        }
        o = isPureNum ? inp : "";
    }
    return o;
}

// Does the node contain a number (eg. 124, 0xf012c, "george")
bool isNumberLike(Node node) {
    if (node.type == ASTNODE) return false;
    return strToNumeric(node.val) != "";
}

//Normalizes number representations
Node nodeToNumeric(Node node) {
    std::string o = strToNumeric(node.val);
    return token(o == "" ? node.val : o, node.metadata);
}

Node tryNumberize(Node node) {
    if (node.type == TOKEN && isNumberLike(node)) return nodeToNumeric(node);
    return node;
}

//Converts a value to an array of byte number nodes
std::vector<Node> toByteArr(std::string val, Metadata metadata, int minLen) {
    std::vector<Node> o;
    int L = 0;
    while (val != "0" || L < minLen) {
        o.push_back(token(decimalMod(val, "256"), metadata));
        val = decimalDiv(val, "256");
        L++;
    }
    std::vector<Node> o2;
    for (int i = o.size() - 1; i >= 0; i--) o2.push_back(o[i]);
    return o2;
}

int counter = 0;

//Makes a unique token
std::string mkUniqueToken() {
    counter++;
    return intToDecimal(counter);
}

//Does a file exist? http://stackoverflow.com/questions/12774207
bool exists(std::string fileName) {
    std::ifstream infile(fileName.c_str());
    return infile.good();
}

//Reads a file: http://stackoverflow.com/questions/2602013
std::string get_file_contents(std::string filename)
{
  std::ifstream in(filename.c_str(), std::ios::in | std::ios::binary);
  if (in)
  {
    std::string contents;
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    return(contents);
  }
  throw(errno);
}

//Report error
void err(std::string errtext, Metadata met) {
    std::string err = "Error (file \"" + met.file + "\", line " +
        intToDecimal(met.ln) + ", char " + intToDecimal(met.ch) +
        "): " + errtext;
    std::cerr << err << "\n";
    throw(err);
}

//Bin to hex
std::string binToHex(std::string inp) {
    std::string o = "";
    for (int i = 0; i < inp.length(); i++) {
        unsigned char v = inp[i];
        o += std::string("0123456789abcdef").substr(v/16, 1)
           + std::string("0123456789abcdef").substr(v%16, 1);
    }
    return o;
}

//Hex to bin
std::string hexToBin(std::string inp) {
    std::string o = "";
    for (int i = 0; i+1 < inp.length(); i+=2) {
        char v = (char)(std::string("0123456789abcdef").find(inp[i]) * 16 +
                std::string("0123456789abcdef").find(inp[i+1]));
        o += v;
    }
    return o;
}