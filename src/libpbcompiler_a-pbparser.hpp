/* A Bison parser, made by GNU Bison 2.7.12-4996.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_PB_LIBPBCOMPILER_A_PBPARSER_HPP_INCLUDED
# define YY_PB_LIBPBCOMPILER_A_PBPARSER_HPP_INCLUDED
/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int pbdebug;
#endif

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     KW_TRANSFORM = 258,
     KW_FROM = 259,
     KW_TO = 260,
     KW_THROUGH = 261,
     KW_LE = 262,
     KW_GE = 263,
     KW_EQ = 264,
     KW_WHERE = 265,
     KW_AND = 266,
     KW_OR = 267,
     KW_PRIMARY = 268,
     KW_SECONDARY = 269,
     KW_PRIORITY = 270,
     KW_ROTATABLE = 271,
     KW_MAIN = 272,
     KW_RECURSIVE = 273,
     KW_DUPLICATE = 274,
     KW_MEMOIZED = 275,
     KW_GENERATOR = 276,
     KW_TEMPLATE = 277,
     KW_TUNABLE = 278,
     KW_CONFIG = 279,
     KW_PARAM = 280,
     KW_ACCURACYMETRIC = 281,
     KW_ACCURACYBINS = 282,
     KW_ACCURACYVARIABLE = 283,
     KW_RULE = 284,
     KW_INPUTFEATURE = 285,
     TOK_INTEGER = 286,
     TOK_FLOAT = 287,
     TOK_RULEBODY = 288,
     IDENT = 289
   };
#endif
/* Tokens.  */
#define KW_TRANSFORM 258
#define KW_FROM 259
#define KW_TO 260
#define KW_THROUGH 261
#define KW_LE 262
#define KW_GE 263
#define KW_EQ 264
#define KW_WHERE 265
#define KW_AND 266
#define KW_OR 267
#define KW_PRIMARY 268
#define KW_SECONDARY 269
#define KW_PRIORITY 270
#define KW_ROTATABLE 271
#define KW_MAIN 272
#define KW_RECURSIVE 273
#define KW_DUPLICATE 274
#define KW_MEMOIZED 275
#define KW_GENERATOR 276
#define KW_TEMPLATE 277
#define KW_TUNABLE 278
#define KW_CONFIG 279
#define KW_PARAM 280
#define KW_ACCURACYMETRIC 281
#define KW_ACCURACYBINS 282
#define KW_ACCURACYVARIABLE 283
#define KW_RULE 284
#define KW_INPUTFEATURE 285
#define TOK_INTEGER 286
#define TOK_FLOAT 287
#define TOK_RULEBODY 288
#define IDENT 289



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{
/* Line 2053 of yacc.c  */
#line 88 "pbparser.ypp"

  int i;
  double d;
  const char* str;
  petabricks::Transform*       transform;
  petabricks::TransformList*   transforms;
  petabricks::MatrixDef*       matrixdef;
  petabricks::MatrixDefList*   matrixdefs;
  petabricks::UserRule*        rule;
  petabricks::RuleList*        rules;
  petabricks::Formula*         formula;
  petabricks::FormulaList*     formulas;
  petabricks::Region*          region;
  petabricks::RegionList*      regions;
  petabricks::OrderedFreeVars* freevars;
  petabricks::TemplateArg*     templatearg;
  petabricks::TemplateArgList* templateargs;
  petabricks::DoubleList*      doublelist;
  petabricks::ConfigItem*      configitem;
  struct { const char* str; petabricks::FormulaList* formulas; } str_formulas; 


/* Line 2053 of yacc.c  */
#line 148 "libpbcompiler_a-pbparser.hpp"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} YYLTYPE;
# define yyltype YYLTYPE /* obsolescent; will be withdrawn */
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE pblval;
extern YYLTYPE pblloc;
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int pbparse (void *YYPARSE_PARAM);
#else
int pbparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int pbparse (petabricks::TransformListPtr& ret);
#else
int pbparse ();
#endif
#endif /* ! YYPARSE_PARAM */

#endif /* !YY_PB_LIBPBCOMPILER_A_PBPARSER_HPP_INCLUDED  */
