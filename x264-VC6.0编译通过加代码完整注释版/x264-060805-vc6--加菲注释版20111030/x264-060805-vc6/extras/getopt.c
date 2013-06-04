/*	$NetBSD: getopt_long.c,v 1.15 2002/01/31 22:43:40 tv Exp $	*/

/*-
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Dieter Baron and Thomas Klausner.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * x264 --crf 22 --profile main --tune animation --preset medium --b-pyramid none -o test.mp4 oldFile.avi
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>

#define REPLACE_GETOPT

#define _DIAGASSERT(x) do {} while (0)	/* 循环只执行一次；在定义宏的时候如有必要（比方使用了if语句）就要使用do{…}while(0)将它封闭起来。 */
										/* http://wmnmtm.blog.163.com/blog/static/38245714201172921555477/ */
#ifdef REPLACE_GETOPT
	#ifdef __weak_alias
	__weak_alias(getopt,_getopt)		/* weak:弱的;alias:别名 */
	#endif
int opterr = 1;		/* if error message should be printed 是否打印错误信息 */
int optind = 1;		/* index into parent argv vector 父参数向量索引 */
int optopt = '?';	/* character(符号) checked for validity(有效) */
int optreset;		/* reset(重置) getopt */
char *optarg;		/* argument(参数) associated(相关的、相关联的) with option(选择) */
#endif

#ifdef __weak_alias
__weak_alias(getopt_long,_getopt_long)
#endif

char *__progname = "x264";

#define IGNORE_FIRST	(*options == '-' || *options == '+')	/* IGNORE:忽略 */
#define PRINT_ERROR	((opterr) && ((*options != ':') || (IGNORE_FIRST && options[1] != ':')))	/* 打印错误 */

#define IS_POSIXLY_CORRECT (getenv("POSIXLY_INCORRECT_GETOPT") == NULL)	/* POSIX:字符集 */

#define PERMUTE         (!IS_POSIXLY_CORRECT && !IGNORE_FIRST)	/* PERMUTE:改变顺序 */
/* XXX: GNU ignores PC if *options == '-' */
#define IN_ORDER        (!IS_POSIXLY_CORRECT && *options == '-')

/* return values 返回值 */
#define	BADCH	(int)'?'
#define	BADARG		((IGNORE_FIRST && options[1] == ':') \
			 || (*options == ':') ? (int)':' : (int)'?')
#define INORDER (int)1	/* INORDER:按顺序 */

static char EMSG[1];

static int getopt_internal (int, char * const *, const char *);
static int gcd (int, int);
static void permute_args (int, int, int, char * const *);	/* permute改变顺序 */

static char *place = EMSG; /* option选项 letter字母 processing(数据)处理, 加工 */

/* XXX: set optreset to 1 rather than these two (rather than :与其…倒不如…)*/
static int nonopt_start = -1; /* first non option argument (for permute) */
static int nonopt_end = -1;   /* first option after non options (for permute序列改变) */

/* 错误信息 Error messages */
static const char recargchar[] = "option requires an argument(选项要求一个参数) -- %c";		/* 错误信息：选项要求一个参数 -- */
static const char recargstring[] = "option requires an argument(选项要求一个参数) -- %s";	/* 错误信息：选项要求一个参数 */
static const char ambig[] = "ambiguous option(含糊不清的选项) -- %.*s";						/* 错误信息：含糊不清的选项 */
static const char noarg[] = "option doesn't take an argument(选项不需要一个参数) -- %.*s";	/* 错误信息：选项不需要一个参数 */
static const char illoptchar[] = "unknown option(未知选项) -- %c";							/* 错误信息：未知选项 */
static const char illoptstring[] = "unknown option(未知选项) -- %s";						/* 错误信息：未知选项 */

static void _vwarnx(const char *fmt, va_list ap)
{
  (void)fprintf(stderr, "加菲_%s: ", __progname);	/* char *__progname = "x264_";(本getopt.c文件前面) */
  if (fmt != NULL)								/* 错误信息 != NULL */
    (void)vfprintf(stderr, fmt, ap);			/* 格式化的数据输出到指定的数据流中 */
  (void)fprintf(stderr, "\n");					/* 打印错误信息 */
}
												/*
												函数名: vfprintf
											　　功 能: 格式化的数据输出到指定的数据流中
											　　用 法: int vfprintf(FILE *stream, char *format, va_list param);
											　　函数说明：vfprintf()会根据参数format字符串来转换并格式化数据，然后将结果输出到参数stream指定的文件中，直到出现字符串结束（‘\0’）为止。关于参数format字符串的格式请参 考printf（）。
											　　返回值：成功则返回实际输出的字符数，失败则返回-1，错误原因存于errno中。
												资料：http://baike.baidu.com/view/1081188.htm
												*/

static void warnx(const char *fmt, ...)					
{
  va_list ap;			/* typedef char *  va_list; 也可能是一个struct (typedef struct {char *a0; int offset; } va_list;) 在STDIO.H中 */
  va_start(ap, fmt);	/* start:必须以va_start开始，并以va_end结尾 */
  _vwarnx(fmt, ap);		/* ap是当前参数的指针 */
  va_end(ap);			/* end	:必须以va_end结尾，把ap指针清为NULL */
}	//在函数体中声明一个va_list，然后用va_start函数来获取参数列表中的参数，使用完毕后调用va_end()结束。
												/*
												http://wmnmtm.blog.163.com/blog/static/382457142011729112244313/ 
												va_start使argp指向第一个可选参数。
												va_arg返回参数列表中的当前参数并使argp指向参数列表中的下一个参数。va_end把argp指针清为NULL。
												函数体内可以多次遍历这些参数，但是都必须以va_start开始，并以va_end结尾。
												*/

/*
 * Compute(计算) the greatest(最大的) common( 共同的) divisor(除数) of a and b.
 * 计算a和b的最大公约数
 * 公约数，亦称“公因数”。它是几个整数同时均能整除的整数。如果一个整数同时是几个整数的约数，称这个整数为它们的“公约数”；公约数中最大的称为最大公约数
 * http://wmnmtm.blog.163.com/blog/static/382457142011730254697/
 */
static int gcd(a, b)
	int a;
	int b;
{
	int c;

	c = a % b;			/* 除模，即a/b的余数 */
	while (c != 0) {	/* 即a不是b的整数倍 */
		a = b;
		b = c;
		c = a % b;
	}

	return b;
}

/*
 * Exchange(交换) the block from nonopt_start to nonopt_end with the block
 * from nonopt_end to opt_end (keeping the same order of arguments in each block).
 * 
 */
static void
permute_args(panonopt_start, panonopt_end, opt_end, nargv)	/* permute:序列改变,广义转置 */
	int panonopt_start;
	int panonopt_end;
	int opt_end;
	char * const *nargv;
{
	int cstart, cyclelen, i, j, ncycle, nnonopts, nopts, pos;
	char *swap;

	_DIAGASSERT(nargv != NULL);

	/*
	 * compute lengths of blocks and number and size of cycles(周期)
	 */
	nnonopts = panonopt_end - panonopt_start;
	nopts = opt_end - panonopt_end;
	ncycle = gcd(nnonopts, nopts);	/* 求最大公约数 */
	cyclelen = (opt_end - panonopt_start) / ncycle;

	for (i = 0; i < ncycle; i++) {
		cstart = panonopt_end+i;
		pos = cstart;
		for (j = 0; j < cyclelen; j++) {
			if (pos >= panonopt_end)
				pos -= nnonopts;
			else
				pos += nopts;
			swap = nargv[pos];
			/* LINTED const cast */
			((char **) nargv)[pos] = nargv[cstart];
			/* LINTED const cast */
			((char **)nargv)[cstart] = swap;
		}
	}
}

/*
 * getopt_internal --
 *	Parse argc/argv argument vector.  Called by user level routines常规的.
 *  Returns -2 if -- is found (can be long option or end of options marker).
 *  http://blog.csdn.net/yui/article/details/5669922
 */
static int
getopt_internal(nargc, nargv, options)	/* 注意这儿，括号中的参数没指定类型，是在括号外定义的 */
	int nargc;							//对应于main()从操作系统来的第1个参数，选项个数
	char * const *nargv;				//对应于main()从操作系统来的第2个参数，命令行选项字符串
	const char *options;
{

	char *oli;				/* option(选项) letter(字母) list(列表) index(索引) */
	int optchar;			/* 选项字符 */

	_DIAGASSERT(nargv != NULL);
	_DIAGASSERT(options != NULL);
	//printf("\n(extras\getopt.c)getopt_internal函数被调用，在此处 options=%s  \n",options);//zjh
	optarg = NULL;

	/*
	 * XXX Some programs (like rsyncd使用) expect预料; 预期 to be able to
	 * XXX re-initialize初始化 optind to 0 and have getopt_long(3)
	 * XXX properly正确地 function again.  Work around this braindamage脑损伤.
	 */
	if (optind == 0)	//选项顺序号，最开始的为1，而不是当作第0个
		optind = 1;		//起始按1，而不是从0索引

	if (optreset)
		nonopt_start = nonopt_end = -1;
start:
	if (optreset || !*place) {		/* 更新扫描指针update scanning pointer */
		optreset = 0;
		if (optind >= nargc) {          /* 如果选项序号大于等于选项的个数，说明处理完了。参数向量的结尾 end of argument vector */
			place = EMSG;
			if (nonopt_end != -1) {
				/* do permutation序列, if we have to */
				permute_args(nonopt_start, nonopt_end,  optind, nargv);
				optind -= nonopt_end - nonopt_start;
			}
			else if (nonopt_start != -1) {
				/*
				 * If we skipped non-options, set optind
				 * to the first of them.
				 */
				optind = nonopt_start;
			}
			nonopt_start = nonopt_end = -1;
			return -1;
		}
		if ((*(place = nargv[optind]) != '-')
		    || (place[1] == '\0')) {    /* found non-option */
			place = EMSG;
			if (IN_ORDER) {
				/*
				 * GNU extension扩展:
				 * return non-option as argument to option 1
				 */
				optarg = nargv[optind++];
				return INORDER;
			}
			if (!PERMUTE) {
				/*
				 * if no permutation序列 wanted, stop parsing
				 * at first non-option
				 */
				return -1;
			}
			/* do permutation序列 */
			if (nonopt_start == -1)
				nonopt_start = optind;
			else if (nonopt_end != -1) {
				permute_args(nonopt_start, nonopt_end,
				    optind, nargv);
				nonopt_start = optind -
				    (nonopt_end - nonopt_start);
				nonopt_end = -1;
			}
			optind++;
			/* process处理 next argument */
			goto start;//goto 开始
		}
		if (nonopt_start != -1 && nonopt_end == -1)
			nonopt_end = optind;
		if (place[1] && *++place == '-') {	/* found "--" */
			place++;
			return -2;
		}
	}
	if ((optchar = (int)*place++) == (int)':' ||
	    (oli = strchr(options + (IGNORE_FIRST ? 1 : 0), optchar)) == NULL) {
		/* option letter字母 unknown or ':' */
		if (!*place)
			++optind;
		if (PRINT_ERROR)
			warnx(illoptchar, optchar);
		optopt = optchar;
		return BADCH;
	}
	if (optchar == 'W' && oli[1] == ';') {		/* -W long-option */
		/* XXX: what if no long options provided (called by getopt)? */
		if (*place)
			return -2;

		if (++optind >= nargc) {	/* no arg */
			place = EMSG;
			if (PRINT_ERROR)
				warnx(recargchar, optchar);
			optopt = optchar;
			return BADARG;
		} else				/* white space */
			place = nargv[optind];
		/*
		 * Handle -W arg the same as --arg (which causes getopt to
		 * stop parsing).
		 */
		return -2;
	}
	if (*++oli != ':') {			/* doesn't take argument */
		if (!*place)
			++optind;
	} else {				/* takes (optional) argument */
		optarg = NULL;
		if (*place)			/* no white space */
			optarg = place;
		/* XXX: disable test for :: if PC? (GNU doesn't) */
		else if (oli[1] != ':') {	/* arg not optional */
			if (++optind >= nargc) {	/* no arg */
				place = EMSG;
				if (PRINT_ERROR)
					warnx(recargchar, optchar);
				optopt = optchar;
				return BADARG;
			} else
				optarg = nargv[optind];
		}
		place = EMSG;
		++optind;
	}
	/* dump back option letter */
	return optchar;
}

#ifdef REPLACE_GETOPT
/*
 * getopt --
 *	Parse argc/argv argument vector.
 *
 * [eventually(最后) this will replace the real getopt]
 */
int
getopt(nargc, nargv, options)
	int nargc;
	char * const *nargv;
	const char *options;
{
	int retval;

	_DIAGASSERT(nargv != NULL);//???????
	_DIAGASSERT(options != NULL);

	if ((retval = getopt_internal(nargc, nargv, options)) == -2) {
		++optind;
		/*
		 * We found an option (--), so if we skipped non-options,
		 * we have to permute.
		 */
		if (nonopt_end != -1) {
			permute_args(nonopt_start, nonopt_end, optind,
				       nargv);
			optind -= nonopt_end - nonopt_start;
		}
		nonopt_start = nonopt_end = -1;
		retval = -1;
	}
	return retval;
}
#endif

/*
 * getopt_long --
 * Parse argc/argv argument vector.
 * 在x264.c中的调用代码如下：
 * int c = getopt_long( argc, argv, "8A:B:b:f:hI:i:m:o:p:q:r:t:Vvw",long_options, &long_options_index); 
*/
int getopt_long(nargc, nargv, options, long_options, idx)//第1个和第2个参数，对应main()从操作系统来的参数
	int nargc;										//第3个参数是冒号分割的字符串,第4个参数是option long_options[]结构体数组，第5个参数是
	char * const *nargv;							//第5个参数是&long_options_index，好像是序号
	const char *options;/* 字符串 */
	const struct option *long_options;/* option类型的指针 */
	int *idx;

{
	int retval;
	int ti;//zjh
	char *tm;
	tm = nargv;
	_DIAGASSERT(nargv != NULL);
	_DIAGASSERT(options != NULL);
	_DIAGASSERT(long_options != NULL);
	/* idx may be NULL */

	/*
	printf("getopt_long函数被调用,本函数第1个参数nargc=%d \n",nargc);//zjh
	for (ti=0;ti<nargc;ti++)
	{
		printf("getopt_long函数被调用,本函数第2个参数nargv=%s \n",*nargv);//zjh
		nargv++;
	}
	*/
//	nargv = tm;//zjh	
//	printf("getopt_long函数被调用,本函数第3个参数options=%s \n",options);//zjh

	if ((retval = getopt_internal(nargc, nargv, options)) == -2) //internal:内部的//前两个参数对应于main(...)，第三个即为冒号分隔的字符串
	{
		char *current_argv, *has_equal;
		size_t current_argv_len;
		int i, match;

		current_argv = place;
		match = -1;

		optind++;
		place = EMSG;

		if (*current_argv == '\0') {		/* found "--" */
			/*
			 * We found an option (--), so if we skipped
			 * non-options, we have to permute.
			 */
			if (nonopt_end != -1) {
				permute_args(nonopt_start, nonopt_end,
				    optind, nargv);
				optind -= nonopt_end - nonopt_start;
			}
			nonopt_start = nonopt_end = -1;
			return -1;
		}
		if ((has_equal = strchr(current_argv, '=')) != NULL) {
			/* argument found (--option=arg) */
			current_argv_len = has_equal - current_argv;
			has_equal++;
		} else
			current_argv_len = strlen(current_argv);

		for (i = 0; long_options[i].name; i++) {//
			/* 查找长选项 find matching long option */
			if (strncmp(current_argv, long_options[i].name,// 串比较;说明:比较字符串str1和str2的大小，如果str1小于str2，返回值就<0，反之如果str1大于str2，返回值就>0，如果str1等于str2，返回值就=0，maxlen指的是str1与str2的比较的字符数。此函数功能即比较字符串str1和str2的前maxlen个字符
			    current_argv_len))//注意for的括号中中间那项，不是最平常的比较，long_options[i].name为真就进行循环
				continue;//两个字符串相等时返回0，执行后续语句，不相等时，进行下次循环，后续语句被跳过了

			if (strlen(long_options[i].name) ==//长选项的name长度==当前参数的长度(因为要一个个检查处理一堆参数，轮到现在这个了)
			    (unsigned)current_argv_len) {//非0，条件就成立，即为真，负数也是非0，也当作条件成立，测试过。http://wmnmtm.blog.163.com/blog/static/38245714201181333128280/
				/* exact准确的, 确切的 match匹配 */
				match = i;
				break;
			}
			if (match == -1)		/* partial不完全的 match匹配 */
				match = i;
			else {
				/* ambiguous(引起歧义的; 模棱两可的, 含糊不清的) abbreviation(缩写; 缩写词) */
				if (PRINT_ERROR)
					warnx(ambig, (int)current_argv_len,
					     current_argv);
				optopt = 0;
				return BADCH;
			}
		}
		if (match != -1) {			/* option found */
			if (long_options[match].has_arg == no_argument
			    && has_equal) {
				if (PRINT_ERROR)
					warnx(noarg, (int)current_argv_len,
					     current_argv);
				/*
				 * XXX: GNU sets optopt to val regardless of
				 * flag
				 */
				if (long_options[match].flag == NULL)
					optopt = long_options[match].val;
				else
					optopt = 0;
				return BADARG;
			}
			if (long_options[match].has_arg == required_argument ||
			    long_options[match].has_arg == optional_argument) {
				if (has_equal)
					optarg = has_equal;
				else if (long_options[match].has_arg ==
				    required_argument) {
					/*
					 * optional argument doesn't use
					 * next nargv
					 */
					optarg = nargv[optind++];
				}
			}
			if ((long_options[match].has_arg == required_argument)
			    && (optarg == NULL)) {
				/*
				 * Missing argument; leading ':'
				 * indicates no error should be generated
				 */
				if (PRINT_ERROR)
					warnx(recargstring, current_argv);
				/*
				 * XXX: GNU sets optopt to val regardless
				 * of flag
				 */
				if (long_options[match].flag == NULL)
					optopt = long_options[match].val;
				else
					optopt = 0;
				--optind;
				return BADARG;
			}
		} else {			/* unknown option */
			if (PRINT_ERROR)
				warnx(illoptstring, current_argv);
			optopt = 0;
			return BADCH;
		}
		if (long_options[match].flag) /* 结构体的第3个字段 != 0 */
		{
			*long_options[match].flag = long_options[match].val;
			retval = 0;
		} else
			retval = long_options[match].val;	/* 结构体数组的第几个元素(是个结构体)的val字段 */
		if (idx)
			*idx = match;//这儿更新了,idx是从x264.c中传来的指针，这儿解引用，把原值给更新了，算是返馈回去了
	}
	return retval;
}
