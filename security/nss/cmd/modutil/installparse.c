/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef lint
char yysccsid[] = "@(#)yaccpar	1.4 (Berkeley) 02/25/90";
#endif
#line 37 "installparse.y"

#define yyparse Pk11Install_yyparse
#define yylex Pk11Install_yylex
#define yyerror Pk11Install_yyerror
#define yychar Pk11Install_yychar
#define yyval Pk11Install_yyval
#define yylval Pk11Install_yylval
#define yydebug Pk11Install_yydebug
#define yynerrs Pk11Install_yynerrs
#define yyerrflag Pk11Install_yyerrflag
#define yyss Pk11Install_yyss
#define yyssp Pk11Install_yyssp
#define yyvs Pk11Install_yyvs
#define yyvsp Pk11Install_yyvsp
#define yylhs Pk11Install_yylhs
#define yylen Pk11Install_yylen
#define yydefred Pk11Install_yydefred
#define yydgoto Pk11Install_yydgoto
#define yysindex Pk11Install_yysindex
#define yyrindex Pk11Install_yyrindex
#define yygindex Pk11Install_yygindex
#define yytable Pk11Install_yytable
#define yycheck Pk11Install_yycheck
#define yyname Pk11Install_yyname
#define yyrule Pk11Install_yyrule

/* C Stuff */
#include "install-ds.h"
#include <prprf.h>

#define YYSTYPE Pk11Install_Pointer
extern char *Pk11Install_yytext;
char *Pk11Install_yyerrstr = NULL;

#line 40 "ytab.c"
#define OPENBRACE 257
#define CLOSEBRACE 258
#define STRING 259
#define YYERRCODE 256
/* clang-format on */
short yylhs[] = {
    -1,
    0, 1, 1, 2, 2, 3, 4,
};
short yylen[] = {
    2,
    1, 2, 0, 1, 1, 4, 1,
};
short yydefred[] = {
    0,
    0, 0, 1, 0, 4, 0, 2, 0, 0, 6,
};
short yydgoto[] = {
    2,
    3, 4, 5, 6,
};
short yysindex[] = {
    -257,
    0, 0, 0, -257, 0, -252, 0, -257, -251, 0,
};
short yyrindex[] = {
    6,
    1, 0, 0, 3, 0, 0, 0, -250, 0, 0,
};
short yygindex[] = {
    0,
    -4, 0, 0, 0,
};
#define YYTABLESIZE 261
short yytable[] = {
    7,
    5, 1, 3, 9, 8, 3, 10, 3, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 7, 5, 5,
    3,
};
short yycheck[] = {
    4,
    0, 259, 0, 8, 257, 0, 258, 258, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 257, 258, 259,
    258,
};
/* clang-format on */
#define YYFINAL 2
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 259
#if YYDEBUG
char *yyname[] = {
    "end-of-file", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "OPENBRACE", "CLOSEBRACE", "STRING",
};
char *yyrule[] = {
    "$accept : toplist",
    "toplist : valuelist",
    "valuelist : value valuelist",
    "valuelist :",
    "value : key_value_pair",
    "value : STRING",
    "key_value_pair : key OPENBRACE valuelist CLOSEBRACE",
    "key : STRING",
};
#endif
#ifndef YYSTYPE
typedef int YYSTYPE;
#endif
#define yyclearin (yychar = (-1))
#define yyerrok (yyerrflag = 0)
#ifndef YYSTACKSIZE
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 300
#endif
#endif
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
#define yystacksize YYSTACKSIZE
short yyss[YYSTACKSIZE];
YYSTYPE yyvs[YYSTACKSIZE];
#line 118 "installparse.y"
/*----------------------- Program Section --------------------------------*/

/*************************************************************************/
void
Pk11Install_yyerror(char *message)
{
    char *tmp;
    if (Pk11Install_yyerrstr) {
        tmp = PR_smprintf("%sline %d: %s\n", Pk11Install_yyerrstr,
                          Pk11Install_yylinenum, message);
        PR_smprintf_free(Pk11Install_yyerrstr);
    } else {
        tmp = PR_smprintf("line %d: %s\n", Pk11Install_yylinenum, message);
    }
    Pk11Install_yyerrstr = tmp;
}
#line 191 "ytab.c"
#define YYABORT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab
int
yyparse()
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register char *yys;
    extern char *PR_GetEnvSecure();

    if ((yys = PR_GetEnvSecure("YYDEBUG")) != NULL) {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate]) != 0)
        goto yyreduce;
    if (yychar < 0) {
        if ((yychar = yylex()) < 0)
            yychar = 0;
#if YYDEBUG
        if (yydebug) {
            yys = 0;
            if (yychar <= YYMAXTOKEN)
                yys = yyname[yychar];
            if (!yys)
                yys = "illegal-symbol";
            printf("yydebug: state %d, reading %d (%s)\n", yystate,
                   yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
        yyn <= YYTABLESIZE && yycheck[yyn] == yychar) {
#if YYDEBUG
        if (yydebug)
            printf("yydebug: state %d, shifting to state %d\n",
                   yystate, yytable[yyn]);
#endif
        if (yyssp >= yyss + yystacksize - 1) {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)
            --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
        yyn <= YYTABLESIZE && yycheck[yyn] == yychar) {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag)
        goto yyinrecovery;
#ifdef lint
    goto yynewerror;
yynewerror:
#endif
    yyerror("syntax error");
#ifdef lint
    goto yyerrlab;
yyerrlab:
#endif
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3) {
        yyerrflag = 3;
        for (;;) {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE) {
#if YYDEBUG
                if (yydebug)
                    printf("yydebug: state %d, error recovery shifting\
 to state %d\n",
                           *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yyss + yystacksize - 1) {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            } else {
#if YYDEBUG
                if (yydebug)
                    printf("yydebug: error recovery discarding state %d\n",
                           *yyssp);
#endif
                if (yyssp <= yyss)
                    goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    } else {
        if (yychar == 0)
            goto yyabort;
#if YYDEBUG
        if (yydebug) {
            yys = 0;
            if (yychar <= YYMAXTOKEN)
                yys = yyname[yychar];
            if (!yys)
                yys = "illegal-symbol";
            printf("yydebug: state %d, error recovery discards token %d (%s)\n",
                   yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("yydebug: state %d, reducing by rule %d (%s)\n",
               yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1 - yym];
    switch (yyn) {
        case 1:
#line 84 "installparse.y"
        {
            Pk11Install_valueList = yyvsp[0].list;
        } break;
        case 2:
#line 89 "installparse.y"
        {
            Pk11Install_ValueList_AddItem(yyvsp[0].list, yyvsp[-1].value);
            yyval.list = yyvsp[0].list;
        } break;
        case 3:
#line 94 "installparse.y"
        {
            yyval.list = Pk11Install_ValueList_new();
        } break;
        case 4:
#line 99 "installparse.y"
        {
            yyval.value = Pk11Install_Value_new(PAIR_VALUE, yyvsp[0]);
        } break;
        case 5:
#line 103 "installparse.y"
        {
            yyval.value = Pk11Install_Value_new(STRING_VALUE, yyvsp[0]);
        } break;
        case 6:
#line 108 "installparse.y"
        {
            yyval.pair = Pk11Install_Pair_new(yyvsp[-3].string, yyvsp[-1].list);
        } break;
        case 7:
#line 113 "installparse.y"
        {
            yyval.string = yyvsp[0].string;
        } break;
#line 374 "ytab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0) {
#ifdef YYDEBUG
        if (yydebug)
            printf("yydebug: after reduction, shifting from state 0 to\
 state %d\n",
                   YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0) {
            if ((yychar = yylex()) < 0)
                yychar = 0;
#if YYDEBUG
            if (yydebug) {
                yys = 0;
                if (yychar <= YYMAXTOKEN)
                    yys = yyname[yychar];
                if (!yys)
                    yys = "illegal-symbol";
                printf("yydebug: state %d, reading %d (%s)\n",
                       YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0)
            goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
        yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#ifdef YYDEBUG
    if (yydebug)
        printf("yydebug: after reduction, shifting from state %d \
to state %d\n",
               *yyssp, yystate);
#endif
    if (yyssp >= yyss + yystacksize - 1) {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
