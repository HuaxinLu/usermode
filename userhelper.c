/* -*-Mode: c-*- */
/* Copyright (C) 1997 Red Hat Software, Inc.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/* 
 * UserTool suid helper program
 */

#if 0
#define USE_MCHECK 1
#endif

#include <assert.h>

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef USE_MCHECK
#include <mcheck.h>
#endif

#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include <pwdb/pwdb_public.h>

#include "userhelper.h"
#include "shvar.h"

/* Total GECOS field length... is this enough ? */
#define GECOS_LENGTH		80

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif


/* ------ some static data objects ------- */

static char *full_name	= NULL; /* full user name */
static char *office	= NULL; /* office */
static char *office_ph	= NULL;	/* office phone */
static char *home_ph	= NULL;	/* home phone */
static char *user_name	= NULL; /* the account name */
static char *shell_path = NULL; /* shell path */

static char *the_username = NULL; /* used to mangle the conversation function */

/* manipulate the environment directly */
extern char **environ;

/* command line flags */
static int 	f_flg = 0; 	/* -f flag = change full name */
static int	o_flg = 0;	/* -o flag = change office name */
static int	p_flg = 0;	/* -p flag = change office phone */
static int 	h_flg = 0;	/* -h flag = change home phone number */
static int 	c_flg = 0;	/* -c flag = change password */
static int 	s_flg = 0;	/* -s flag = change shell */
static int 	t_flg = 0;	/* -t flag = direct text-mode -- exec'ed */
static int 	w_flg = 0;	/* -w flag = act as a wrapper for next args */

/*
 * A handy fail exit function we can call from man places
 */
static int fail_error(int retval)
{
  /* this is a temporary kludge.. will be fixed shortly. */
    if(retval == ERR_SHELL_INVALID)
        exit(ERR_SHELL_INVALID);	  

    if (retval != PAM_SUCCESS) {
	switch(retval) {
	    case PAM_AUTH_ERR:
	    case PAM_PERM_DENIED:
		exit (ERR_PASSWD_INVALID);
	    case PAM_AUTHTOK_LOCK_BUSY:
		exit (ERR_LOCKS);
	    case PAM_CRED_INSUFFICIENT:
	    case PAM_AUTHINFO_UNAVAIL:
		exit (ERR_NO_RIGHTS);
	    case PAM_ABORT:
	    default:
		exit(ERR_UNK_ERROR);
	}
    }
    exit (0);
}

/*
 * Read a string from stdin, returns a malloced memory to it
 */
static char *read_string(void)
{
    char *buffer = NULL;
    char *check = NULL;
    int slen = 0;
    
    buffer = (char *) malloc(BUFSIZ);
    if (buffer == NULL)
	return NULL;
    
    check = fgets(buffer, BUFSIZ, stdin);
    if (!check)
	return NULL;
    slen = strlen(buffer);
    if (buffer[slen-1] == '\n')
	buffer[slen-1] = '\0';
    return buffer;
}

/*
 * Conversation function for the boring change password stuff
 */
static int conv_func(int num_msg, const struct pam_message **msg,
		     struct pam_response **resp, void *appdata_ptr)
{
    int 	count = 0;
    struct pam_response *reply = NULL;
    char *noecho_message;

    reply = (struct pam_response *)
	calloc(num_msg, sizeof(struct pam_response));
    if (reply == NULL)
	return PAM_CONV_ERR;
    
    /*
     * We do first a pass on all items and output them;
     * then we do a second pass and read what we have to read
     * from stdin
     */
    for (count = 0; count < num_msg; count++) {
	switch (msg[count]->msg_style) {
	    case PAM_PROMPT_ECHO_ON:
		printf("%d %s\n", UH_ECHO_ON_PROMPT, msg[count]->msg);
		break;
	    case PAM_PROMPT_ECHO_OFF:
		if (the_username && !strncasecmp(msg[count]->msg, "password", 8)) {
		    noecho_message = alloca (strlen (the_username) + 14);
		    assert(noecho_message);
		    sprintf(noecho_message, "%s's password:", the_username);
		} else {
		    noecho_message = msg[count]->msg;
		}
		printf("%d %s\n", UH_ECHO_OFF_PROMPT, noecho_message);
		break;
	    case PAM_TEXT_INFO:
		printf("%d %s\n", UH_INFO_MSG, msg[count]->msg);
		break;
	    case PAM_ERROR_MSG:
		printf("%d %s\n", UH_ERROR_MSG, msg[count]->msg);
		break;
	    default:
		printf("0 %s\n", msg[count]->msg);
	}
    }

    /* tell the other side how many messages we sent and how many
     * responses we expect (ignoring messages, which we fudge here).
     */
    printf("%d %d", UH_EXPECT_RESP, num_msg);

    /* now the second pass */
    for (count = 0; count < num_msg; count++) {
	switch (msg[count]->msg_style) {
	    case PAM_PROMPT_ECHO_ON:
		reply[count].resp_retcode = PAM_SUCCESS;
		reply[count].resp = read_string();
		/* PAM frees resp */
		break;
	    case PAM_PROMPT_ECHO_OFF:
		reply[count].resp_retcode = PAM_SUCCESS;
		reply[count].resp = read_string();
		/* PAM frees resp */
		break;
	    case PAM_TEXT_INFO:
		/* ignore it... */
		break;
	    case PAM_ERROR_MSG:
		/* also ignore it... */
		break;
		
	    default:
		/* Must be an error of some sort... */
		free (reply);
		return PAM_CONV_ERR;
	}
    }
    if (reply)
	*resp = reply;
    return PAM_SUCCESS;
}

/*
 * the structure pointing at the conversation function for
 * auth and changing the password
 */
static struct pam_conv pipe_conv = {
     conv_func,
     NULL
};
static struct pam_conv text_conv = {
     misc_conv,
     NULL
};
    
/*
 * A function to process already existing gecos information
 */
static void process_gecos(char *gecos)
{
    char *idx;
    
    if (gecos == NULL)
	return;

    if (!full_name)
	full_name = gecos;    
    idx = strchr(gecos, ',');
    if (idx != NULL) {
	*idx = '\0';
	gecos = idx+1;
    }
    if ((idx == NULL) || (*gecos == '\0')) {
	/* no more fields */
	return;
    }

    if (!office)
	office = gecos;    
    idx = strchr(gecos, ',');
    if (idx != NULL) {
	*idx = '\0';
	gecos = idx+1;
    }

    if (!office_ph)
	office_ph = gecos;
    idx = strchr(gecos, ',');
    if (idx != NULL) {
	*idx = '\0';
	gecos = idx+1;
    }
    if ((idx == NULL) || (*gecos == '\0')) {
	/* no more fields */
	return;
    }
    
    if (!home_ph)
	home_ph = gecos;
    idx = strchr(gecos, ',');
    if (idx != NULL) {
	*idx = '\0';
    }
}

/*
 * invalid_field - insure that a field contains all legal characters
 *
 * The supplied field is scanned for non-printing and other illegal
 * characters.  If any illegal characters are found, invalid_field
 * returns -1.  Zero is returned for success.
 */

int invalid_field(const char *field, const char *illegal)
{
    const char *cp;

    for (cp = field; *cp && isprint (*cp) && ! strchr (illegal, *cp); cp++)
	;
    if (*cp)
	return -1;
    else
	return 0;
}

/*
 * A simple function to compute the gecos field size
 */
static int gecos_size(void)
{
    int len = 0;
    
    if (full_name != NULL)
	len += strlen(full_name);
    if (office != NULL)
	len += strlen(office);
    if (office_ph != NULL)
	len += strlen(office_ph);
    if (home_ph != NULL)
	len += strlen(home_ph);
    return len;
}

/* Snagged straight from the util-linux source... May want to clean up
 * a bit and possibly merge with the code in userinfo that parses to
 * get a list.  -Otto
 *
 *  get_shell_list () -- if the given shell appears in /etc/shells,
 *      return true.  if not, return false.
 *      if the given shell is NULL, /etc/shells is outputted to stdout.
 */
static int get_shell_list(char* shell_name)
{
    FILE *fp;
    int found;
    int len;
    static char buf[1024];

    found = FALSE;
    fp = fopen ("/etc/shells", "r");
    if (! fp) {
        if (! shell_name) fprintf (stderr, "No known shells.\n");
        return FALSE;
    }
    while (fgets (buf, sizeof (buf), fp) != NULL) {
        /* ignore comments */
        if (*buf == '#') continue;
        len = strlen (buf);
        /* strip the ending newline */
        if (buf[len - 1] == '\n') buf[len - 1] = 0;
        /* ignore lines that are too damn long */
        else continue;
        /* check or output the shell */
        if (shell_name) {
            if (! strcmp (shell_name, buf)) {
	        found = TRUE;
                break;
            }
        }
        else printf ("%s\n", buf);
    }
    fclose (fp);
    return found;
}

#ifdef USE_MCHECK
void
mcheck_out(enum mcheck_status reason) {
    char *explanation;

    switch (reason) {
	case MCHECK_DISABLED:
	    explanation = "Consistency checking is not turned on."; break;
	case MCHECK_OK:
	    explanation = "Block is fine."; break;
	case MCHECK_FREE:
	    explanation = "Block freed twice."; break;
	case MCHECK_HEAD:
	    explanation = "Memory before the block was clobbered."; break;
	case MCHECK_TAIL:
	    explanation = "Memory after the block was clobbered."; break;
    }
    printf("%d %s\n", UH_ERROR_MSG, explanation);
    printf("%d 1", UH_EXPECT_RESP);
}
#endif

/* ------- the application itself -------- */
int main(int argc, char *argv[])
{
    int		arg;
    int 	retval;
    char	*progname = NULL;
    pam_handle_t 	*pamh = NULL;
    struct passwd	*pw;
    struct pam_conv     *conv;

#ifdef USE_MCHECK
    mtrace();
    mcheck(mcheck_out);
#endif

    /* for lack of a better place to put it... */
    setbuf(stdout, NULL);
    setbuf(stdin, NULL);

    if (geteuid() != 0) {
	fprintf(stderr, "userhelper must be setuid root\n");
	exit(ERR_NO_RIGHTS);
    }

    while (!w_flg && (arg = getopt(argc, argv, "f:o:p:h:cs:tw:")) != -1) {
	/* we process no arguments after -w progname; those are sacred */
	switch (arg) {
	    case 'f':
		f_flg++; full_name = optarg;
		break;
	    case 'o':
		o_flg++; office = optarg;
		break;
	    case 'h':
		h_flg++; home_ph = optarg;
		break;
	    case 'p':
		p_flg++; office_ph = optarg;
		break;
	    case 'c':
		c_flg++;
		break;
	    case 's':
		s_flg++; shell_path = optarg;
		break;
	    case 't':
		t_flg++;
		break;
	    case 'w':
		w_flg++; progname = optarg;
		break;
	    default:
		exit(ERR_INVALID_CALL);
	}
    } 
    /* verify the parameters a little */
#   define SHELL_FLAGS (f_flg || o_flg || p_flg || h_flg || s_flg)
    if ((c_flg && SHELL_FLAGS) || (c_flg && w_flg) || (w_flg && SHELL_FLAGS))
	exit(ERR_INVALID_CALL);

    /* point to the right conversation function */
    conv = t_flg ? &text_conv : &pipe_conv;
    
    /* now try to identify the username we are doing all this work for */
    user_name = getlogin();
    if (user_name == NULL) {
	struct passwd *tmp;
	
	tmp = getpwuid(getuid());
	if (tmp != (struct passwd *)NULL) {
	    user_name = tmp->pw_name;
	    if (user_name != NULL)
		user_name = strdup(user_name);
	    else
		/* weirdo, bail out */
		exit (ERR_UNK_ERROR);
	}
    }
    if ((getuid() == 0) && (argc == optind+1)) {
	/* we have a username supplied on the command line and
	 * the guy running this program is the root */
	user_name = argv[optind];
    }

    /* check for the existance of this user */
    pw = getpwnam(user_name);
    if (pw == (struct passwd *)NULL) {
	/* user not known */
	exit(ERR_NO_USER);
    }

    /* I guess we don't need this anymore */
    endpwent();

    /* okay, start the process */    
    if (c_flg) { /* are we changing the password ? */

	/* Start the PAM session, recommend ourselves as 'passwd' */
	retval = pam_start("passwd", user_name, conv, &pamh);
	if (retval != PAM_SUCCESS)
	    fail_error(retval);

	/* Change the password */
	retval = pam_chauthtok(pamh, 0);
	if (retval != PAM_SUCCESS)
	    fail_error(retval);

    } else if (w_flg) { /* we are a wrapper program */
	/* pick the first existing of /usr/sbin/<progname> and /sbin/<progname>
	 * authenticate <progname>
	 * argv[optind-1] = <progname> (boondoggle unless -- used)
	 * (we know there is at least one open slot in argv for this)
	 * execv(constructed_path, argv+optind);
	 */
	char *constructed_path;
	char *apps_filename;
	char *user, *apps_user;
	char *retry;
	char *env_home, *env_term, *env_display, *env_shell, *env_user, *env_logname;
	int session, fallback, try;
	size_t aft;
	struct stat sbuf;
	shvarFile *s;

	if (strrchr(progname, '/'))
	    progname = strrchr(progname, '/');

	env_home = getenv("HOME");
	env_term = getenv("TERM");
	env_display = getenv("DISPLAY");
	/* note that XAUTHORITY not copied -- do not let attackers get at
	 * others' X authority records
	 */
	env_shell = getenv("SHELL");
	env_user = getenv("USER");
	env_logname = getenv("LOGNAME");

	if (strstr(env_home, ".."))
	    env_home=NULL;
	if (strstr(env_shell, ".."))
	    env_shell=NULL;
	if (strstr(env_term, ".."))
	    env_term="dumb";

	environ = (char **) malloc (2 * sizeof (char *));
	if (env_home) setenv("HOME", env_home, 1);
	if (env_term) setenv("TERM", env_term, 1);
	if (env_display) setenv("DISPLAY", env_display, 1);
	if (env_shell) setenv("SHELL", env_shell, 1);
	if (env_user) setenv("USER", env_user, 1);
	if (env_logname) setenv("LOGNAME", env_logname, 1);
	setenv("PATH",
	       "/usr/sbin:/usr/bin:/sbin:/bin:/usr/X11R6/bin:/root/bin", 1);

	aft = strlen(progname) + sizeof("/etc/security/console.apps/") + 2;
	apps_filename = alloca(aft);
	snprintf(apps_filename, aft, "/etc/security/console.apps/%s", progname);
	s = svNewFile(apps_filename);
	
	if (fstat(s->fd, &sbuf) ||
	    !S_ISREG(sbuf.st_mode) ||
	    (sbuf.st_mode & S_IWOTH))
		exit(ERR_UNK_ERROR); /* don't be verbose about security
					problems; it can help the attacker */

	apps_user = svGetValue(s, "USER");
	if (!apps_user || !strcmp(apps_user, "<user>")) {
	    user = user_name;
	} else {
	    user = apps_user;
	}

	constructed_path = svGetValue(s, "PROGRAM");
	if (!constructed_path || constructed_path[0] != '/') {
	    constructed_path = malloc(strlen(progname) + sizeof("/usr/sbin/") + 2);
	    if (!constructed_path) exit (ERR_NO_MEMORY);

	    strcpy(constructed_path, "/usr/sbin/");
	    strcat(constructed_path, progname);
	    if (access(constructed_path, X_OK)) {
		strcpy(constructed_path, "/sbin/");
		strcat(constructed_path, progname);
		if (access(constructed_path, X_OK)) {
		    exit (ERR_NO_PROGRAM);
		}
	    }
	}

	session = svTrueValue(s, "SESSION", 0);
	fallback = svTrueValue(s, "FALLBACK", 0);
	retry = svGetValue(s, "RETRY");
	if (retry) {
	    try = atoi(retry) + 1;
	} else {
	    try = 3;
	}

	svCloseFile(s);

	the_username = user;
	retval = pam_start(progname, user, conv, &pamh);
	if (retval != PAM_SUCCESS)
	    fail_error(retval);

	retval = !PAM_SUCCESS;
	while (try-- && retval != PAM_SUCCESS) {
	    retval = pam_authenticate(pamh, 0);
	}
	if (retval != PAM_SUCCESS) {
	    pam_end(pamh, retval);
	    if (fallback) {
		setuid(getuid());
		argv[optind-1] = progname;
		execv(constructed_path, argv+optind-1);
		exit (ERR_EXEC_FAILED);
	    } else {
		fail_error(retval);
	    }
	}

	retval = pam_acct_mgmt(pamh, 0);
	if (retval != PAM_SUCCESS) {
	    pam_end(pamh, retval);
	    fail_error(retval);
	}

	if (session) {
	    int child, status;

	    retval = pam_open_session(pamh, 0);
	    if (retval != PAM_SUCCESS) {
		pam_end(pamh, retval);
		fail_error(retval);
	    }

	    if (!(child = fork())) {
		struct passwd *pw;

		setuid(0);
		pw = getpwuid(getuid());
		if (pw) setenv("HOME", pw->pw_dir, 1);
		argv[optind-1] = progname;
		execv(constructed_path, argv+optind-1);
		exit (ERR_EXEC_FAILED);
	    }

	    wait4 (child, &status, 0, NULL);

	    retval = pam_close_session(pamh, 0);
	    if (retval != PAM_SUCCESS) {
		pam_end(pamh, retval);
		fail_error(retval);
	    }

	    if (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) {
		pam_end(pamh, PAM_SUCCESS);
		retval = 1;
	    } else {
		pam_end(pamh, PAM_SUCCESS);
		retval = ERR_EXEC_FAILED;
	    }
	    exit (retval);

	} else {
	    /* this is not a session, so do not do session management */

	    pam_end(pamh, PAM_SUCCESS);

	    /* time for an exec */
	    setuid(0);
	    argv[optind-1] = progname;
	    execv(constructed_path, argv+optind-1);
	    exit (ERR_EXEC_FAILED);
	}

    } else { /* we are changing some gecos fields */

	char	new_gecos[GECOS_LENGTH+10]; 
	pwdb_type user_unix[2] = { PWDB_UNIX, _PWDB_MAX_TYPES };
	const struct pwdb *_pwdb = NULL;
	const struct pwdb_entry *_pwe = NULL;
	int try=3;
	
	/* verify the fields we were passed */
	if (f_flg && invalid_field(full_name, ":,="))
	    exit(ERR_FIELDS_INVALID);
	if (o_flg && invalid_field(office, ":,="))
	    exit(ERR_FIELDS_INVALID);
	if (p_flg && invalid_field(office_ph, ":,="))
	    exit(ERR_FIELDS_INVALID);
	if (h_flg && invalid_field(home_ph, ":,="))
	    exit(ERR_FIELDS_INVALID);
    
	retval = pam_start("chfn", user_name, conv, &pamh);
	if (retval != PAM_SUCCESS)
	    fail_error(retval);

	retval = !PAM_SUCCESS;
	while (try-- && retval != PAM_SUCCESS) {
	    retval = pam_authenticate(pamh, 0);
	}
	if (retval != PAM_SUCCESS) {
	    pam_end(pamh, retval);
	    fail_error(retval);
	}
	
	retval = pam_acct_mgmt(pamh, 0);
	if (retval != PAM_SUCCESS) {
	    pam_end(pamh, retval);
	    fail_error(retval);
	}
	
	/* Now update the user entry */
	pwdb_start();
	retval = pwdb_locate("user", user_unix, user_name, PWDB_ID_UNKNOWN, &_pwdb);
	if (retval != PWDB_SUCCESS) {
	    pam_end(pamh, PAM_ABORT);
	    fail_error(PAM_ABORT);
	}

	retval = pwdb_get_entry(_pwdb, "gecos", &_pwe);
	if (retval == PWDB_SUCCESS) {
	    /* we have a gecos entry already... process it */
	    process_gecos(_pwe->value);
	}
     
	/* verify that the strigs we get passed are not too long */	
	if (gecos_size() > GECOS_LENGTH) {
	    pwdb_entry_delete(&_pwe);
	    pwdb_delete(&_pwdb);
	    pwdb_end();
	    pam_end(pamh, PAM_ABORT);
	    exit(ERR_FIELDS_INVALID);
	}
	/* Build the new gecos field */
	snprintf(new_gecos, sizeof(new_gecos),
		 "%s,%s,%s,%s",
		 full_name ? full_name : "",
		 office ? office : "",
		 office_ph ? office_ph : "",
		 home_ph ? home_ph : "");

	/* We don't need the pwdb_entry for gecos anymore... */
	pwdb_entry_delete(&_pwe);
	
	retval = pwdb_set_entry(_pwdb, "gecos", new_gecos, 1+strlen(new_gecos),
				NULL, NULL, 0);
	if (retval != PWDB_SUCCESS) {
	    /* try to exit cleanely */
	    pwdb_delete(&_pwdb);
	    pwdb_end();
	    pam_end(pamh, PAM_ABORT);
	    fail_error(PAM_ABORT);
	}

	/* if we change the shell too ... */
	if (s_flg != 0) {

	  /* check that the users current shell is valid... */
	  retval = pwdb_get_entry(_pwdb, "shell", &_pwe);
	  if(!get_shell_list(shell_path) || 
	     !get_shell_list((char*)_pwe->value) ||
	     retval != PWDB_SUCCESS) {
	    fail_error(ERR_SHELL_INVALID);
	    pwdb_entry_delete(&_pwe);
	  }
	    pwdb_entry_delete(&_pwe);

	    retval = pwdb_set_entry(_pwdb, "shell", shell_path, 1+strlen(shell_path),
				    NULL, NULL, 0);
	    if (retval != PWDB_SUCCESS) {
		/* try a clean exit */
		pwdb_delete(&_pwdb);
		pwdb_end();
		pam_end(pamh, PAM_ABORT);
		fail_error(PAM_ABORT);
	    }
	}    
	retval = pwdb_replace("user", _pwdb->source, user_name, PWDB_ID_UNKNOWN, &_pwdb);
	if (retval != PWDB_SUCCESS) {
	    /* clean exit ... */
	    pwdb_delete(&_pwdb);
	    pwdb_end();
	    pam_end(pamh, PAM_ABORT);
	    fail_error(PAM_ABORT);
	}
	pwdb_delete(&_pwdb);
	pwdb_end();	
    }

    /* all done */     
    if (pamh != NULL)
	retval = pam_end(pamh, PAM_SUCCESS);
    if (retval != PAM_SUCCESS)
	fail_error(retval);
    exit (0);
}
