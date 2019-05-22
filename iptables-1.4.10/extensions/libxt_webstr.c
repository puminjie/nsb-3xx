/* Shared library add-on to iptables to add webstr matching support. 
 *
 * Copyright (C) 2003, CyberTAN Corporation
 * All Rights Reserved.
 *
 * Description:
 *   This is shared library, added to iptables, for web content inspection. 
 *   It was derived from 'string' matching support, declared as above.
 *
 */

/* iptable-1.4.10 webstr patch. (ipt->xt)
 *
 * Copyright (C) 2011, Richerlink Technology Corporation
 * All Rights Reserved.
 */

#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
//#include <linux/netfilter_ipv4/ipt_webstr.h>

#define BM_MAX_NLEN 256
#define BM_MAX_HLEN 1024

#define BLK_JAVA        0x01
#define BLK_ACTIVE      0x02
#define BLK_COOKIE      0x04
#define BLK_PROXY       0x08

typedef char *(*proc_ipt_search) (char *, char *, int, int);

struct ipt_webstr_info {
    char string[BM_MAX_NLEN];
    u_int16_t invert;
    u_int16_t len;
    u_int8_t type;
};

enum xt_webstr_type
{
    IPT_WEBSTR_HOST,
    IPT_WEBSTR_URL,
    IPT_WEBSTR_CONTENT,
    IPT_WEBSTR_ALL
};



/* Function which prints out usage message. */
static void
help(void)
{
	printf(
"WEBSTR match v%s options:\n"
"webstr [!] --host            Match a http string in a packet\n"
"webstr [!] --url             Match a http string in a packet\n"
"webstr [!] --content         Match a http string in a packet\n"
"webstr [!] --all             Match all http packets\n",
IPTABLES_VERSION);

	fputc('\n', stdout);
}

static struct option opts[] = {
	{ "host", 1, 0, '1' },
	{ "url", 1, 0, '2' },
	{ "content", 1, 0, '3' },
	{ "all", 1, 0, '4' },
	{0}
};

/* Initialize the match. */
static void
init(struct ipt_entry_match *m)
{
	return;
}

static void
parse_string(char *s, struct ipt_webstr_info *info)
{	
        if (strlen(s) <= BM_MAX_NLEN) strcpy(info->string, s);
	else xtables_error(PARAMETER_PROBLEM, "WEBSTR too long `%s'", s);
}

/* Function which parses command options; returns true if it
   ate an option */
static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const void *entry,
#if 0
      unsigned int *nfcache,
#endif
      struct ipt_entry_match **match)
{
	struct ipt_webstr_info *stringinfo = (struct ipt_webstr_info *)(*match)->data;

	switch (c) {
	case '1':
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		parse_string(argv[optind-1], stringinfo);
		if (invert)
			stringinfo->invert = 1;
                stringinfo->len=strlen((char *)&stringinfo->string);
                stringinfo->type = IPT_WEBSTR_HOST;
		break;

	case '2':
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		parse_string(argv[optind-1], stringinfo);
		if (invert)
			stringinfo->invert = 1;
                stringinfo->len=strlen((char *)&stringinfo->string);
                stringinfo->type = IPT_WEBSTR_URL;
		break;

	case '3':
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		parse_string(argv[optind-1], stringinfo);
		if (invert)
			stringinfo->invert = 1;
                stringinfo->len=strlen((char *)&stringinfo->string);
                stringinfo->type = IPT_WEBSTR_CONTENT;
		break;

	case '4':
		xtables_check_inverse(optarg, &invert, &optind, 0, argv);
		parse_string(argv[optind-1], stringinfo);
		if (invert)
			stringinfo->invert = 1;
                stringinfo->len=strlen((char *)&stringinfo->string);
                stringinfo->type = IPT_WEBSTR_ALL;
		break;
		
	default:
		return 0;
	}

	*flags = 1;
	return 1;
}

static void
print_string(char string[], int invert, int numeric)
{

	if (invert)
		fputc('!', stdout);
	printf("%s ",string);
}

/* Final check; must have specified --string. */
static void
final_check(unsigned int flags)
{
	if (!flags)
		xtables_error(PARAMETER_PROBLEM,
			   "WEBSTR match: You must specify `--webstr'");
}

/* Prints out the matchinfo. */
static void
print(const void *ip,
      const struct ipt_entry_match *match,
      int numeric)
{
	struct ipt_webstr_info *stringinfo = (struct ipt_webstr_info *)match->data;

	printf("WEBSTR match ");

	
	switch (stringinfo->type) {
	case IPT_WEBSTR_HOST:
		printf("host ");
		break;

	case IPT_WEBSTR_URL:
		printf("url ");
		break;

	case IPT_WEBSTR_CONTENT:
		printf("content ");
		break;

	case IPT_WEBSTR_ALL:
		printf("all ");
		break;
	
	default:
		printf("error ");
		break;
	}

	print_string(((struct ipt_webstr_info *)match->data)->string,
		  ((struct ipt_webstr_info *)match->data)->invert, numeric);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void
save(const void *ip, const struct ipt_entry_match *match)
{
	printf("--webstr ");
	print_string(((struct ipt_webstr_info *)match->data)->string,
		  ((struct ipt_webstr_info *)match->data)->invert, 0);
}

static
struct xtables_match webstr = { 
    .next = NULL,
    .name = "webstr",
    .family = NFPROTO_UNSPEC,	
    .version = XTABLES_VERSION,
    .size = XT_ALIGN(sizeof(struct ipt_webstr_info)),
    .userspacesize = XT_ALIGN(sizeof(struct ipt_webstr_info)),
    .help = help,
    .init = init,
    .parse = parse,
    .final_check = final_check,
    .print = print,
    .save = save,
    .extra_opts = opts
};

void _init(void)
{
	xtables_register_match(&webstr);
}
