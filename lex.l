%{
#include"scanner.h"
%}

%%
"\n"|";"                        return NEXT_LINE;
"|"                             return PIPE;
"<"                             |
"<<"                            |
">"                             |
">>"                            |
"2>"                            |
"2>>"                           return REDIRECTION;
" "
"\\"                           return SEPERATOR;
[^"|""<""<<"">"">>""2>""2>>"" "";""\n""\\"]*   return COMMAND;
\"+.*+\"                                         return COMMAND;

%%

int yywrap() 
{ 
   return 1; 
} 
