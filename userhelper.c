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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include <pwdb/pwdb_public.h>

/* Error codes we have to return */
#define	ERR_PASSWD_INVALID	1	/* password is not right */
#define ERR_FIELDS_INVALID	2	/* gecos fields invalid or
					 * sum(lengths) too big */
#define ERR_SET_PASSWORD	3	/* password resetting error */
#define ERR_LOCKS		4	/* some files are locked */
#define	ERR_NO_USER		5	/* user unknown ... */
#define ERR_NO_RIGHTS		6	/* insufficient rights to perform operation */
#define ERR_INVALID_CALL	7	/* invalid call to this program */
#define ERR_UNK_ERROR		255	/* unknown error */
					   
/* Total GECOS field length... is this enough ? */
#define GECOS_LENGTH		80

/* ------ some static data objects ------- */

static char *full_name	= NULL; /* full user name */
static char *office	= NULL; /* office */
static char *office_ph	= NULL;	/* office phone */
static char *home_ph	= NULL;	/* home phone */
static char *user_name	= NULL; /* the account name */

/* command line flags */
static int 	f_flg = 0; 	/* -f flag = change full name */
static int	o_flg = 0;	/* -o flag = change office name */
static int	p_flg = 0;	/* -p flag = change office phone */
static int 	h_flg = 0;	/* -h flag = change home phone number */
static int 	c_flg = 0;	/* -c flag = change password */

/*
 * A handy fail exit function we can call from man places
 */
static int fail_error(int retval)
{
    fprintf(stderr, "failing with error: %s\n",
	    pam_strerror(retval));
    
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
    if (check != buffer)
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
    int		replies = 0;
    struct pam_response *reply = NULL;
    int size = sizeof(struct pam_response);

#define GET_MEM \
    if (reply) realloc(reply, size); \
    else reply = malloc(size); \
    if (!reply) return PAM_CONV_ERR; \
    size += sizeof(struct pam_response);
#define COPY_STRING(s) (s) ? strdup(s) : NULL

    /*
     * We do first a pass on all items and output them;
     * then we do a second pass and read what we have to read
     * from stdin
     */
    for (count = 0; count < num_msg; count++) {
	switch (msg[count]->msg_style) {
	    case PAM_PROMPT_ECHO_ON:
		printf("1 %s\n", msg[count]->msg);
		break;
	    case PAM_PROMPT_ECHO_OFF:
		printf("2 %s\n", msg[count]->msg);
		break;
	    case PAM_TEXT_INFO:
		printf("3 %s\n", msg[count]->msg);
		break;
	    case PAM_ERROR_MSG:
		printf("4 %s\n", msg[count]->msg);
		break;
	    default:
		printf("0 %s\n", msg[count]->msg);
	}
    }

    /* now the second pass */
    for (count = 0; count < num_msg; count++) {
	switch (msg[count]->msg_style) {
	    case PAM_PROMPT_ECHO_ON:
		GET_MEM;
		reply[replies].resp_retcode = PAM_SUCCESS;
		reply[replies++].resp = read_string();
		/* PAM frees resp */
		break;
	    case PAM_PROMPT_ECHO_OFF:
		GET_MEM;
		reply[replies].resp_retcode = PAM_SUCCESS;
		reply[replies++].resp = read_string();
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
static struct pam_conv conv = {
     conv_func,
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

/* ------- the application itself -------- */
int main(int argc, char *argv[])
{
    int		arg;
    int 	retval;
    pam_handle_t 	*pamh = NULL;
    struct passwd	*pw;
     
    if (geteuid() != 0) {
	fprintf(stderr, "Hmm, we need root privs for this program...\n");
	exit(ERR_NO_RIGHTS);
    }

    while ((arg = getopt(argc, argv, "f:o:p:h:c")) != EOF) {
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
	    default:
		exit(ERR_INVALID_CALL);
	}
    } 
    /* verify a little the parameters */
    if (c_flg && (f_flg || o_flg || p_flg || h_flg))
	exit(ERR_INVALID_CALL);
    
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

    /* check for the existance of this looser */
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
	retval = pam_start("passwd", user_name, &conv, &pamh);
	if (retval != PAM_SUCCESS)
	    fail_error(retval);

	/* Change the password */
	retval = pam_chauthtok(pamh, 0);
	if (retval != PAM_SUCCESS)
	    fail_error(retval);

    } else { /* we are changing some gecos fields */

	char	new_gecos[GECOS_LENGTH+10]; 
	pwdb_type user_unix[2] = { PWDB_UNIX, _PWDB_MAX_TYPES };
	const struct pwdb *_pwdb = NULL;
	const struct pwdb_entry *_pwe = NULL;
	
	/* verify the fields we were passed */
	if (f_flg && invalid_field(full_name, ":,="))
	    exit(ERR_FIELDS_INVALID);
	if (o_flg && invalid_field(office, ":,="))
	    exit(ERR_FIELDS_INVALID);
	if (p_flg && invalid_field(office_ph, ":,="))
	    exit(ERR_FIELDS_INVALID);
	if (h_flg && invalid_field(home_ph, ":,="))
	    exit(ERR_FIELDS_INVALID);
    
	retval = pam_start("chfn", user_name, &conv, &pamh);
	if (retval != PAM_SUCCESS)
	    fail_error(retval);

	retval = pam_authenticate(pamh, 0);
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

