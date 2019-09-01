/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * mod_aron_extra.c --
 *
 */
#include <switch.h>

#define SET_RAW_LONG_DESC "Set a channel variable for the channel calling the application without expanding the value."
#define GET_VAR_EXPANDED_SYNTAX "get_var_expanded <varname>"
#define SPLIT_ARRAY_SYNTAX "join([prefix_each=my-prefix,joiner='|',expand=1,expand_each=1,splitby=',',max_split=1], this-is-my-array,which-i-would-like-to-split,and-join)"

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_aron_extra_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_aron_extra_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_aron_extra_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime)
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_aron_extra, mod_aron_extra_load, mod_aron_extra_shutdown, NULL);

#define ESCAPE_META '\\'

/* Helper function used when separating strings to unescape a character. The
   supported characters are:

   \n  linefeed
   \r  carriage return
   \t  tab
   \s  space

   Any other character is returned as it was received. */
static char unescape_char(char escaped)
{
    char unescaped;

    switch (escaped) {
        case 'n':
            unescaped = '\n';
            break;
        case 'r':
            unescaped = '\r';
            break;
        case 't':
            unescaped = '\t';
            break;
        case 's':
            unescaped = ' ';
            break;
        default:
            unescaped = escaped;
    }
    return unescaped;
}

/* Helper function used when separating strings to remove quotes, leading /
   trailing spaces, and to convert escaped characters. */
static char *cleanup_separated_string(char *str, char delim)
{
    char *ptr;
    char *dest;
    char *start;
    char *end = NULL;
    int inside_quotes = 0;

    /* Skip initial whitespace */
    for (ptr = str; *ptr == ' '; ++ptr) {
    }

    for (start = dest = ptr; *ptr; ++ptr) {
        char e;
        int esc = 0;

        if (*ptr == ESCAPE_META) {
            e = *(ptr + 1);
            if (e == '\'' || e == '"' || (delim && e == delim) || e == ESCAPE_META || (e = unescape_char(*(ptr + 1))) != *(ptr + 1)) {
                ++ptr;
                *dest++ = e;
                end = dest;
                esc++;
            }
        }
        if (!esc) {
            if (*ptr == '\'' && (inside_quotes || ((ptr+1) && strchr(ptr+1, '\'')))) {
                if ((inside_quotes = (1 - inside_quotes))) {
                    end = dest;
                }
            } else {
                *dest++ = *ptr;
                if (*ptr != ' ' || inside_quotes) {
                    end = dest;
                }
            }
        }
    }
    if (end) {
        *end = '\0';
    }

    return start;
}

unsigned int separate_string_string_delim(char *buf, const char *delim, char **array, unsigned int arraylen, int strip_whitespace)
{
    enum tokenizer_state {
        START,
        FIND_DELIM
    } state = START;

    unsigned int count = 0;
    char *ptr = buf;
    int inside_quotes = 0;
    unsigned int i;
    int delim_len = strlen(delim);

    while (*ptr && count < arraylen) {

        switch (state) {
            case START:
                if (strip_whitespace) {
                    while (isspace(*ptr)) {
                        ptr++;
                    }
                    if (!*ptr) {
                        break;
                    }
                }
                array[count++] = ptr;
                state = FIND_DELIM;
                break;

            case FIND_DELIM:
                /* escaped characters are copied verbatim to the destination string */
                if (*ptr == ESCAPE_META) {
                    ptr++;
                } else if (*ptr == '\'' && (inside_quotes || ((ptr+1) && strchr(ptr+1, '\'')))) {
                    inside_quotes = (1 - inside_quotes);
                } else if (*ptr == *delim && !inside_quotes) {
                    for (i = 0; i < delim_len; i++) {
                        if (ptr[i] != delim[i]) {
                            break;
                        }
                    }

                    if (i == delim_len && ptr[i]) {
                        *ptr = '\0';
                        ptr += i - 1;
                        state = START;
                    }
                }
                ++ptr;
                break;
        }
    }
    /* strip quotes, escaped chars and leading / trailing spaces */

    for (i = 0; i < count; ++i) {
        array[i] = cleanup_separated_string(array[i], 0);
    }

    return count;
}



static void base_set (switch_core_session_t *session, const char *data, switch_stack_t stack, switch_bool_t expand)
{
    char *var, *val = NULL;
    const char *what = "SET";

    switch (stack) {
        case SWITCH_STACK_PUSH:
            what = "PUSH";
            break;
        case SWITCH_STACK_UNSHIFT:
            what = "UNSHIFT";
            break;
        default:
            break;
    }

    if (zstr(data)) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No variable name specified.\n");
    } else {
        switch_channel_t *channel = switch_core_session_get_channel(session);
        char *expanded = NULL;

        var = switch_core_session_strdup(session, data);

        if (!(val = strchr(var, '='))) {
            val = strchr(var, ',');
        }

        if (val) {
            *val++ = '\0';
            if (zstr(val)) {
                val = NULL;
            }
        }

        if (val) {
            if (expand) {
                expanded = switch_channel_expand_variables(channel, val);
            } else {
                expanded = val;
            }
        }

        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "RAW:%s %s [%s]=[%s]\n",
                          what, switch_channel_get_name(channel), var, expanded ? expanded : "UNDEF");

        switch_channel_add_variable_var_check(channel, var, expanded, SWITCH_FALSE, stack);

        if (expanded && expanded != val) {
            switch_safe_free(expanded);
        }
    }
}


SWITCH_STANDARD_APP(set_raw_function)
{
    base_set(session, data, SWITCH_STACK_BOTTOM, SWITCH_FALSE);
}

SWITCH_STANDARD_APP(set_array_using_delim_function)
{
    char *p = NULL, *e, *var = NULL, *val = NULL;
    char delim = ' ';
    int argc;
    char *argv[25] = { 0 };
    int i;
    switch_channel_t *channel = switch_core_session_get_channel(session);
    int numopts;
    char *opts[10] = { 0 };

    int expand = 0;
    int expand_each = 0;
    char *expanded = NULL;

    if (zstr(data)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No variable name specified");
        return;
    }

    var = switch_core_session_strdup(session, data);
    p = var;

    if (p && *p == '[') {
        e = switch_find_end_paren(p, '[', ']');
        if (e) {
            *e++ = '\0';
            var = e;

            numopts = switch_separate_string(p, ',', opts, switch_arraylen(opts));
            for (i = 0; i < numopts; i++) {
                char *opt_name = opts[i];

                if (!strncmp(opt_name, "delim=", 6)) {
                    delim = *(opt_name+6);
                } else if (!strcmp(opt_name, "expand")) {
                    expand = 1;
                } else if (!strcmp(opt_name, "expand-each")) {
                    expand_each = 1;
                }
            }
        }
    }

    if (zstr(var)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No variable name specified after options []");
        return;
    }

    if (!(val = strchr(var, '='))) {
        val = strchr(var, ',');
    }

    if (val) {
        *val++ = '\0';
        if (zstr(val)) {
            val = NULL;
        }
    }

    if (val) {
        if (expand) {
            expanded = switch_channel_expand_variables(channel, val);
        } else {
            expanded = val;
        }

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SET_ARRAY: Separating var %s by '%c'\n\n", var, delim);

        argc = switch_separate_string(val, delim, argv, switch_arraylen(argv));

        for (i = 0; i < argc; i++) {
            char *val_i = argv[i];
            if (expand_each) {
                val_i = switch_channel_expand_variables(channel, val_i);
            }

            if (!zstr(val_i)) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SET_ARRAY: setting %s[%d] = %s\n\n", var, i, val_i);
                switch_channel_add_variable_var_check(channel, var, val_i, SWITCH_FALSE, SWITCH_STACK_PUSH);
            }

            if (val_i && val_i != argv[i]) {
                switch_safe_free(val_i);
            }
        }

        if (expanded && expanded != val) {
            switch_safe_free(expanded);
        }
    }

}


SWITCH_STANDARD_API(join_array_function)
{
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_channel_t *channel = NULL;
    char *var, *expanded = NULL;
    char *dup = NULL, *p, *e;
    int numopts;
    char *opts[20] = { 0 };
    int expand = 0;
    int expand_each = 0;
    int i;
    const char *split_by = ":|";
    int max_split = 0;
    const char *joiner = "";
    const char *prefix_first = "";
    const char *prefix_last = "";
    const char *prefix_each = "";
    const char *suffix_first = "";
    const char *suffix_last = "";
    const char *suffix_each = "";
    int strip_white_space = 0;
    char *array[100] = { 0 };
    int array_len = 0;

    if (session) {
        channel = switch_core_session_get_channel(session);
    }

    if (zstr(cmd)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No variable name\n");
        return SWITCH_STATUS_FALSE;
    }

    dup = strdup(cmd);
    p = dup;

    if (p && *p == '[') {
        e = switch_find_end_paren(p++, '[', ']');
        if (e) {
            *e++ = '\0';

            if (!(var = strchr(e, ','))) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Syntax error: missing second arg\n");
                switch_goto_status(SWITCH_STATUS_FALSE, cleanup);
            }

            numopts = switch_separate_string(p, ',', opts, switch_arraylen(opts));
            for (i = 0; i < numopts; i++) {
                char *opt_name = opts[i];

                if (!strcmp(opt_name, "expand")) {
                    if (!channel) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot expand variables without a session\n");
                        switch_goto_status(SWITCH_STATUS_FALSE, cleanup);
                    }
                    expand = 1;
                } else if (!strcmp(opt_name, "expand_each")) {
                    if (!channel) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot expand variables without a session\n");
                        switch_goto_status(SWITCH_STATUS_FALSE, cleanup);
                    }
                    expand_each = 1;
                }  else if (!strcmp(opt_name, "strip_white_space")) {
                    strip_white_space = 1;
                } else if (!strncmp(opt_name, "split_by=", 9)) {
                    split_by = opt_name + 9;
                } else if (!strncmp(opt_name, "max_split=", 10)) {
                    max_split = atoi(opt_name + 10);
                } else if (!strncmp(opt_name, "joiner=", 7)) {
                    joiner = opt_name + 7;
                } else if (!strncmp(opt_name, "prefix_first=", 13)) {
                    prefix_first = opt_name + 13;
                } else if (!strncmp(opt_name, "prefix_last=", 12)) {
                    prefix_last = opt_name + 12;
                } else if (!strncmp(opt_name, "prefix_each=", 12)) {
                    prefix_each = opt_name + 12;
                } else if (!strncmp(opt_name, "suffix_first=", 13)) {
                    suffix_first = opt_name + 13;
                } else if (!strncmp(opt_name, "suffix_last=", 12)) {
                    suffix_last = opt_name + 12;
                } else if (!strncmp(opt_name, "suffix_each=", 12)) {
                    suffix_each = opt_name + 12;
                } else {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid option [%s] specified\n", opt_name);
                }
            }

            var++;

            if (strip_white_space) {
                while (isspace(*var)) var++;
                if (*var) {
                    char *end = (var + strlen(var) - 1);
                    while (isspace(*end)) {
                        *end-- = '\0';
                    }
                }
            }

            if (zstr(var)) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Empty value, nothing to do!\n");
                // TODO strem.write("")
                switch_goto_status(SWITCH_STATUS_SUCCESS, cleanup);
            }

            if (expand) {
                expanded = switch_channel_expand_variables(channel, var);
            } else {
                expanded = var;
            }

            if (!zstr(split_by)) {
                int actual_max_split = (max_split > 0 && max_split < switch_arraylen(array)) ? max_split : switch_arraylen(array);
                array_len = separate_string_string_delim(expanded, split_by, array, actual_max_split, strip_white_space);

                if (actual_max_split == array_len && (max_split > actual_max_split || max_split == 0)) {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error: too many items in array. max of %d exceeded\n", actual_max_split);
                    switch_goto_status(SWITCH_STATUS_FALSE, cleanup);
                }

                if (array_len > 0) {
                    char *val_i = array[0];

                    if (expand_each) {
                        val_i = switch_channel_expand_variables(channel, val_i);
                    }

                    if (array_len > 1) {
                        stream->write_function(stream, "%s%s%s%s%s", prefix_first, prefix_each,  val_i ? val_i : "", suffix_each, suffix_first);
                    } else {
                        stream->write_function(stream, "%s%s%s%s%s%s%s",
                                prefix_first, prefix_each, prefix_last,
                                val_i ? val_i : "",
                                suffix_each, suffix_first, suffix_last
                        );
                    }

                    if (val_i && val_i != array[0]) {
                        switch_safe_free(val_i);
                    }

                    for (i = 1; i < array_len-1; i++) {
                        val_i = array[i];

                        if (expand_each) {
                            val_i = switch_channel_expand_variables(channel, val_i);
                        }

                        stream->write_function(stream, "%s%s%s%s", joiner, prefix_each, val_i ? val_i : "", suffix_each);

                        if (val_i && val_i != array[i]) {
                            switch_safe_free(val_i);
                        }
                    }

                    if (array_len > 1) {
                        val_i = array[i];

                        if (expand_each) {
                            val_i = switch_channel_expand_variables(channel, val_i);
                        }

                        stream->write_function(stream, "%s%s%s%s%s%s", joiner, prefix_last, prefix_each,  val_i ? val_i : "", suffix_each, suffix_last);

                        if (val_i && val_i != array[i]) {
                            switch_safe_free(val_i);
                        }
                    }

                } else {
                    stream->write_function(stream, "");
                }
            } else {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "missing split_by option\n");
            }

            if (expanded && expanded != var) {
                switch_safe_free(expanded);
            }

            goto cleanup;

        } else {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Syntax error: missing ']'\n");
            switch_goto_status(SWITCH_STATUS_FALSE, cleanup);
        }

    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Syntax error: missing '[' in first arg\n");
        switch_goto_status(SWITCH_STATUS_FALSE, cleanup);
    }

    cleanup:

    switch_safe_free(dup);
    return status;

}



SWITCH_STANDARD_API(get_var_expanded_function)
{
    const char *expanded = NULL, *val = NULL, *var = cmd;
    switch_channel_t *channel;

    if (!session) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot retrieve variable without a session\n");
        return SWITCH_STATUS_FALSE;
    }

    if (zstr(var)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No variable name\n");
        return SWITCH_STATUS_FALSE;
    }

    channel = switch_core_session_get_channel(session);
    if ((val = switch_channel_get_variable(channel, var))) {
        expanded = switch_channel_expand_variables(channel, val);
    }

    stream->write_function(stream, "%s", expanded ? expanded : "");

    return SWITCH_STATUS_SUCCESS;
}


/* Macro expands to: switch_status_t mod_aron_extra_load(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_aron_extra_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *api_interface;
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

    SWITCH_ADD_APP(app_interface, "set_raw", "Set a channel variable without expanding value", SET_RAW_LONG_DESC,
            set_raw_function, "<varname>=<value>", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);

    SWITCH_ADD_APP(app_interface, "set_array", "Set a channel variable to an array of values", SET_RAW_LONG_DESC,
                   set_array_using_delim_function, "[[delim=,expand,expand-each]]<varname>=<value>[,value]",
                   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC | SAF_ZOMBIE_EXEC);

    SWITCH_ADD_API(api_interface, "get_var_expanded", "Get a channel variable, and expand vars",
            get_var_expanded_function, GET_VAR_EXPANDED_SYNTAX);

    SWITCH_ADD_API(api_interface, "join_array", "Join an array split by split_by and join by joiner",
                   join_array_function, SPLIT_ARRAY_SYNTAX);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_aron_extra_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_aron_extra_shutdown)
{
	return SWITCH_STATUS_SUCCESS;
}


/*
  If it exists, this is called in it's own thread when the module-load completes
  If it returns anything but SWITCH_STATUS_TERM it will be called again automatically
  Macro expands to: switch_status_t mod_aron_extra_runtime()
SWITCH_MODULE_RUNTIME_FUNCTION(mod_aron_extra_runtime)
{
	while(looping)
	{
		switch_cond_next();
	}
	return SWITCH_STATUS_TERM;
}
*/

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */
