#ifndef __PARSER__
#define __PARSER__

class     Parser{
public:
    Parser();
    ~Parser();
  void        init();
  int         parser(String);
  String      getWordPool(int);
  int         getWordCount();
  boolean     isInputEcho();
  boolean     isMacroEcho();
private:
  char        lastCharOfString(String);
  String      removelastChar(String);
  boolean     isDelimiter(char);
  void        addWordPool(String);
  String      dropDelimiter(String);
  char        quotedCharacter(char);
  String      parserBody(String);
  void        appendAliasPool();
  void        setEchoMode(String s);
  boolean     isAliasCommandLine();
  boolean     isEchoCommandLine();
  int         parserEntry(String);
/*
 * bit0 : Echo input text
 * bit1 : Echo macro proccessed text
 */
  int         echoMode = 0;   // Means no echo
};

#endif
