/* dircproxy
 * Copyright (C) 2000 Scott James Remnant <scott@netsplit.com>.
 * All Rights Reserved.
 *
 * cfgfile.c
 *  - reading of configuration file
 * --
 * @(#) $Id: cfgfile.c,v 1.16 2000/09/26 11:31:39 keybuk Exp $
 *
 * This file is distributed according to the GNU General Public
 * License.  For full details, read the top of 'main.c' or the
 * file called COPYING that was distributed with this code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dircproxy.h>
#include "sprintf.h"
#include "irc_net.h"
#include "cfgfile.h"

/* forward declaration */
static int _cfg_read_bool(char **, int *);
static int _cfg_read_numeric(char **, long *);
static int _cfg_read_string(char **, char **);

/* Whitespace */
#define WS " \t\r\n"

/* Quick and easy "Unmatched Quote" define */
#define UNMATCHED_QUOTE { error("Unmatched quote for key '%s' " \
                                "at line %ld of %s", key, line, \
                                filename); valid = 0; break; }

/* Read a config file */
int cfg_read(const char *filename, char **listen_port) {
  struct ircconnclass *class;
  int valid;
  long line;
  FILE *fd;

  char *server_port;
  long server_retry;
  long server_dnsretry;
  long server_maxattempts;
  long server_maxinitattempts;
  long server_pingtimeout;
  long channel_rejoin;
  int disconnect_existing;
  int disconnect_on_detach;
  char *drop_modes;
  char *local_address;
  char *away_message;
  char *chan_log_dir;
  int chan_log_always;
  int chan_log_timestamp;
  long chan_log_maxsize;
  long chan_log_recall;
  char *other_log_dir;
  int other_log_always;
  int other_log_timestamp;
  long other_log_maxsize;
  long other_log_recall;
  int motd_logo;
  int motd_stats;

  class = 0;
  line = 0;
  valid = 1;
  fd = fopen(filename, "r");
  if (!fd)
    return -1;

  /* Initialise using defaults */
  server_port = x_strdup(DEFAULT_SERVER_PORT);
  server_retry = DEFAULT_SERVER_RETRY;
  server_dnsretry = DEFAULT_SERVER_DNSRETRY;
  server_maxattempts = DEFAULT_SERVER_MAXATTEMPTS;
  server_maxinitattempts = DEFAULT_SERVER_MAXINITATTEMPTS;
  server_pingtimeout = DEFAULT_SERVER_PINGTIMEOUT;
  channel_rejoin = DEFAULT_CHANNEL_REJOIN;
  disconnect_existing = DEFAULT_DISCONNECT_EXISTING;
  disconnect_on_detach = DEFAULT_DISCONNECT_ON_DETACH;
  drop_modes = x_strdup(DEFAULT_DROP_MODES);
  local_address = (DEFAULT_LOCAL_ADDRESS ? x_strdup(DEFAULT_LOCAL_ADDRESS) : 0);
  away_message = (DEFAULT_AWAY_MESSAGE ? x_strdup(DEFAULT_AWAY_MESSAGE) : 0);
  chan_log_dir = (DEFAULT_CHAN_LOG_DIR ? x_strdup(DEFAULT_CHAN_LOG_DIR) : 0);
  chan_log_always = DEFAULT_CHAN_LOG_ALWAYS;
  chan_log_timestamp = DEFAULT_CHAN_LOG_TIMESTAMP;
  chan_log_maxsize = DEFAULT_CHAN_LOG_MAXSIZE;
  chan_log_recall = DEFAULT_CHAN_LOG_RECALL;
  other_log_dir = (DEFAULT_OTHER_LOG_DIR ? x_strdup(DEFAULT_OTHER_LOG_DIR) : 0);
  other_log_always = DEFAULT_OTHER_LOG_ALWAYS;
  other_log_timestamp = DEFAULT_OTHER_LOG_TIMESTAMP;
  other_log_maxsize = DEFAULT_OTHER_LOG_MAXSIZE;
  other_log_recall = DEFAULT_OTHER_LOG_RECALL;
  motd_logo = DEFAULT_MOTD_LOGO;
  motd_stats = DEFAULT_MOTD_STATS;

  while (valid) {
    char buff[512], *buf;

    if (!fgets(buff, 512, fd))
      break;
    line++;

    buf = buff;
    while ((buf < (buff + 512)) && strlen(buf)) {
      char *key;

      /* Skip whitespace, and ignore lines that are comments */
      buf += strspn(buf, WS);
      if (*buf == '#')
        break;

      /* Find the end of the key, and if there isn't one, exit */
      key = buf;
      buf += strcspn(buf, WS);
      if (!strlen(key))
        break;

      /* If there isn't a newline, then we could reach the end of the
         buffer - so be a bit careful and ensure we don't skip past it */
      if (*buf) {
        *(buf++) = 0;

        /* Close brace is only allowed when class is defined
           (we check here, because its a special case and has no value) */
        if (!strcmp(key, "}") && !class) {
          error("Close brace without open at line %ld of %s", line, filename);
          valid = 0;
          break;
        }

        buf += strspn(buf, WS);
      }

      /* If we reached the end of the buffer, or a comment, that means
         this key has no value.  Unless the key is '}' then thats bad */
      if ((!*buf || (*buf == '#')) && strcmp(key, "}")) {
        error("Missing value for key '%s' at line %ld of %s",
              key, line, filename);
        valid = 0;
        break;
      }

      /* Handle the keys */
      if (!class && !strcasecmp(key, "listen_port")) {
        /* listen_port 57000
           listen_port "dircproxy"    # From /etc/services
         
           ( cannot go in a connection {} ) */
        char *str;

        if (_cfg_read_string(&buf, &str))
          UNMATCHED_QUOTE;

        /* Make sure the silly programmer supplied the pointer! */
        if (listen_port) {
          free(*listen_port);
          *listen_port = str;
        }

      } else if (!strcasecmp(key, "server_port")) {
        /* server_port 6667
           server_port "irc"    # From /etc/services */
        char *str;

        if (_cfg_read_string(&buf, &str))
          UNMATCHED_QUOTE;

        if (class) {
          free(class->server_port);
          class->server_port = str;
        } else {
          free(server_port);
          server_port = str;
        }

      } else if (!strcasecmp(key, "server_retry")) {
        /* server_retry 15 */
        _cfg_read_numeric(&buf,
                          &(class ? class->server_retry : server_retry));

      } else if (!strcasecmp(key, "server_dnsretry")) {
        /* server_dnsretry 15 */
        _cfg_read_numeric(&buf,
                          &(class ? class->server_dnsretry : server_dnsretry));

      } else if (!strcasecmp(key, "server_maxattempts")) {
        /* server_maxattempts 0 */
        _cfg_read_numeric(&buf,
                          &(class ? class->server_maxattempts
                                  : server_maxattempts));

      } else if (!strcasecmp(key, "server_maxinitattempts")) {
        /* server_maxinitattempts 5 */
        _cfg_read_numeric(&buf,
                          &(class ? class->server_maxinitattempts
                                  : server_maxinitattempts));
        
      } else if (!strcasecmp(key, "server_pingtimeout")) {
        /* server_pingtimeout 600 */
        _cfg_read_numeric(&buf,
                          &(class ? class->server_pingtimeout
                                  : server_pingtimeout));

      } else if (!strcasecmp(key, "channel_rejoin")) {
        /* channel_rejoin 5 */
        _cfg_read_numeric(&buf,
                          &(class ? class->channel_rejoin : channel_rejoin));
 
      } else if (!strcasecmp(key, "disconnect_existing_user")) {
        /* disconnect_existing_user yes
           disconnect_existing_user no */
        _cfg_read_bool(&buf,
                       &(class ? class->disconnect_existing
                               : disconnect_existing));

      } else if (!strcasecmp(key, "disconnect_on_detach")) {
        /* disconnect_on_detach yes
           disconnect_on_detach no */
        _cfg_read_bool(&buf,
                       &(class ? class->disconnect_on_detach
                               : disconnect_on_detach));

      } else if (!strcasecmp(key, "drop_modes")) {
        /* drop_modes "ow" */
        char *str;

        if (_cfg_read_string(&buf, &str))
          UNMATCHED_QUOTE;

        while ((*str == '+') || (*str == '-')) {
          char *tmp;
          
          tmp = str;
          str = x_strdup(tmp + 1);
          free(tmp);
        }

        if (class) {
          free(class->drop_modes);
          class->drop_modes = str;
        } else {
          free(drop_modes);
          drop_modes = str;
        }

      } else if (!strcasecmp(key, "local_address")) {
        /* local_address none
           local_address "i.am.a.virtual.host.com" */
        char *str;

        if (_cfg_read_string(&buf, &str))
          UNMATCHED_QUOTE;

        if (!strcasecmp(str, "none")) {
          free(str);
          str = 0;
        }

        if (class) {
          free(class->local_address);
          class->local_address = str;
        } else {
          free(local_address);
          local_address = str;
        }

      } else if (!strcasecmp(key, "away_message")) {
        /* away_message none
         * away_message "Not available, messages are logged" */
        char *str;

        if (_cfg_read_string(&buf, &str))
          UNMATCHED_QUOTE;

        if (!strcasecmp(str, "none")) {
          free(str);
          str = 0;
        }

        if (class) {
          free(class->away_message);
          class->away_message = str;
        } else {
          free(away_message);
          away_message = str;
        }
        
      } else if (!strcasecmp(key, "chan_log_dir")) {
        /* chan_log_dir none
           chan_log_dir "/log"
           chan_log_dir "~/logs" */
        char *str;

        if (_cfg_read_string(&buf, &str))
          UNMATCHED_QUOTE;

        if (!strcasecmp(str, "none")) {
          free(str);
          str = 0;

        } else if (!strncmp(str, "~/", 2)) {
          char *home;

          home = getenv("HOME");
          if (home) {
            char *tmp;

            tmp = x_sprintf("%s%s", home, str + 1);
            free(str);
            str = tmp;
          } else {
            /* Best we can do */
            *str = '.';
          }
        }

        if (class) {
          free(class->chan_log_dir);
          class->chan_log_dir = str;
        } else {
          free(chan_log_dir);
          chan_log_dir = str;
        }

      } else if (!strcasecmp(key, "chan_log_always")) {
        /* chan_log_always yes
           chan_log_always no */
        _cfg_read_bool(&buf,
                       &(class ? class->chan_log_always : chan_log_always));

      } else if (!strcasecmp(key, "chan_log_timestamp")) {
        /* chan_log_timestamp yes
           chan_log_timestamp no */
        _cfg_read_bool(&buf,
                       &(class ? class->chan_log_timestamp
                               : chan_log_timestamp));

      } else if (!strcasecmp(key, "chan_log_maxsize")) {
        /* chan_log_maxsize 128
           chan_log_maxsize 0 */
        _cfg_read_numeric(&buf,
                          &(class ? class->chan_log_maxsize
                                  : chan_log_maxsize));

      } else if (!strcasecmp(key, "chan_log_recall")) {
        /* chan_log_recall 128
           chan_log_recall 0
           chan_log_recall -1 */
        _cfg_read_numeric(&buf,
                          &(class ? class->chan_log_recall : chan_log_recall));

      } else if (!strcasecmp(key, "other_log_dir")) {
        /* other_log_dir none
           other_log_dir "/log"
           other_log_dir "~/logs" */
        char *str;

        if (_cfg_read_string(&buf, &str))
          UNMATCHED_QUOTE;

        if (!strcasecmp(str, "none")) {
          free(str);
          str = 0;
          
        } else if (!strncmp(str, "~/", 2)) {
          char *home;

          home = getenv("HOME");
          if (home) {
            char *tmp;

            tmp = x_sprintf("%s%s", home, str + 1);
            free(str);
            str = tmp;
          } else {
            /* Best we can do */
            *str = '.';
          }
        }

        if (class) {
          free(class->other_log_dir);
          class->other_log_dir = str;
        } else {
          free(other_log_dir);
          other_log_dir = str;
        }

      } else if (!strcasecmp(key, "other_log_always")) {
        /* other_log_always yes
           other_log_always no */
        _cfg_read_bool(&buf,
                       &(class ? class->other_log_always : other_log_always));

      } else if (!strcasecmp(key, "other_log_timestamp")) {
        /* other_log_timestamp yes
           other_log_timestamp no */
        _cfg_read_bool(&buf,
                       &(class ? class->other_log_timestamp
                               : other_log_timestamp));

      } else if (!strcasecmp(key, "other_log_maxsize")) {
        /* other_log_maxsize 128
           other_log_maxsize 0 */
        _cfg_read_numeric(&buf,
                          &(class ? class->other_log_maxsize
                                  : other_log_maxsize));

      } else if (!strcasecmp(key, "other_log_recall")) {
        /* other_log_recall 128
           other_log_recall 0
           other_log_recall -1 */
        _cfg_read_numeric(&buf,
                          &(class ? class->other_log_recall
                                  : other_log_recall));

      } else if (!strcasecmp(key, "motd_logo")) {
        /* motd_logo yes
           motd_logo no */
        _cfg_read_bool(&buf, &(class ? class->motd_logo : motd_logo));

      } else if (!strcasecmp(key, "motd_stats")) {
        /* motd_stats yes
           motd_stats no */
        _cfg_read_bool(&buf, &(class ? class->motd_stats : motd_stats));

      } else if (!class && !strcasecmp(key, "connection")) {
        /* connection {
             :
             :
           } */

        if (*buf != '{') {
          /* Connection is a bit special, because the only valid value is '{'
             the internals are handled elsewhere */
          error("Expected open brace for key '%s' at line %ld of %s",
                key, line, filename);
          valid = 0;
          break;
        }
        buf++;

        /* Allocate memory, it'll be filled later */
        class = (struct ircconnclass *)malloc(sizeof(struct ircconnclass));
        memset(class, 0, sizeof(struct ircconnclass));
        class->server_port = x_strdup(server_port);
        class->server_retry = server_retry;
        class->server_dnsretry = server_dnsretry;
        class->server_maxattempts = server_maxattempts;
        class->server_maxinitattempts = server_maxinitattempts;
        class->server_pingtimeout = server_pingtimeout;
        class->channel_rejoin = channel_rejoin;
        class->disconnect_existing = disconnect_existing;
        class->disconnect_on_detach = disconnect_on_detach;
        class->drop_modes = x_strdup(drop_modes);
        class->local_address = (local_address ? x_strdup(local_address) : 0);
        class->away_message = (away_message ? x_strdup(away_message) : 0);
        class->chan_log_dir = (chan_log_dir ? x_strdup(chan_log_dir) : 0);
        class->chan_log_always = chan_log_always;
        class->chan_log_timestamp = chan_log_timestamp;
        class->chan_log_maxsize = chan_log_maxsize;
        class->chan_log_recall = chan_log_recall;
        class->other_log_dir = (other_log_dir ? x_strdup(other_log_dir) : 0);
        class->other_log_always = other_log_always;
        class->other_log_timestamp = other_log_timestamp;
        class->other_log_maxsize = other_log_maxsize;
        class->other_log_recall = other_log_recall;
        class->motd_logo = motd_logo;
        class->motd_stats = motd_stats;

      } else if (class && !strcasecmp(key, "password")) {
        /* connection {
             :
             password "foo"
             :
           } */
        char *str;

        if (_cfg_read_string(&buf, &str))
          UNMATCHED_QUOTE;

        free(class->password);
        class->password = str;

      } else if (class && !strcasecmp(key, "server")) {
        /* connection {
             :
             server "irc.linux.com"
             server "irc.linux.com:6670"     # Port other than default
             server "irc.linux.com:6670:foo" # Port and password
             server "irc.linux.com::foo"     # Password and default port
             :
           } */
        struct strlist *s;
        char *str;

        if (_cfg_read_string(&buf, &str))
          UNMATCHED_QUOTE;

        s = (struct strlist *)malloc(sizeof(struct strlist));
        s->str = str;
        s->next = 0;

        if (class->servers) {
          struct strlist *ss;

          ss = class->servers;
          while (ss->next)
            ss = ss->next;

          ss->next = s;
        } else {
          class->servers = s;
        }

      } else if (class && !strcasecmp(key, "from")) {
        /* connection {
             :
             from "static-132.myisp.com"    # Static hostname
             from "*.myisp.com"             # Masked hostname
             from "192.168.1.1"             # Specific IP
             from "192.168.*"               # IP range
             :
           } */
        struct strlist *s;
        char *str;

        if (_cfg_read_string(&buf, &str))
          UNMATCHED_QUOTE;

        s = (struct strlist *)malloc(sizeof(struct strlist));
        s->str = str;
        s->next = class->masklist;
        class->masklist = s;

      } else if (class && !strcmp(key, "}")) {
        /* Check that a password and at least one server were defined */
        if (!class->password) {
          error("Connection class defined without password "
                "before line %ld of %s", line, filename);
          valid = 0;
        } else if (!class->servers) {
          error("Connection class defined without a server "
                "before line %ld of %s", line, filename);
          valid = 0;
        }

        /* Add to the list of servers if valid, otherwise free it */
        if (valid) {
          class->next_server = class->servers;
          class->next = connclasses;
          connclasses = class;
          class = 0;
        } else {
          ircnet_freeconnclass(class);
          class = 0;
          break;
        }

      } else {
        /* Bad key! */
        error("Unknown config file key '%s' at line %ld of %s",
              key, line, filename);
        valid = 0;
        break;
      }

      /* Skip whitespace.  The only things that can trail are comments
         and close braces, and we re-pass to do those */
      buf += strspn(buf, WS);
      if (*buf && (*buf != '#') && (*buf != '}')) {
        error("Unexpected data at and of line %ld of %s", line, filename);
        valid = 0;
        break;
      }
    }
  }

  /* Argh, untidy stuff left around */
  if (class) {
    ircnet_freeconnclass(class);
    class = 0;
    if (valid) {
      error("Unmatched open brace in %s", filename);
      valid = 0;
    }
  }

  fclose(fd);
  free(server_port);
  free(drop_modes);
  free(local_address);
  free(away_message);
  free(chan_log_dir);
  free(other_log_dir);
  return (valid ? 0 : -1);
}

/* Read a boolean value */
static int _cfg_read_bool(char **buf, int *val) {
  char *ptr;

  ptr = *buf;
  *buf += strcspn(*buf, WS);
  *((*buf)++) = 0;

  if (!strcasecmp(ptr, "yes")) {
    *val = 1;
  } else if (!strcasecmp(ptr, "true")) {
    *val = 1;
  } else if (!strcasecmp(ptr, "y")) {
    *val = 1;
  } else if (!strcasecmp(ptr, "t")) {
    *val = 1;
  } else if (!strcasecmp(ptr, "no")) {
    *val = 0;
  } else if (!strcasecmp(ptr, "false")) {
    *val = 0;
  } else if (!strcasecmp(ptr, "n")) {
    *val = 0;
  } else if (!strcasecmp(ptr, "f")) {
    *val = 0;
  } else {
    *val = (atoi(ptr) ? 1 : 0);
  }

  return 0;
}

/* Read a numeric value */
static int _cfg_read_numeric(char **buf, long *val) {
  char *ptr;

  ptr = *buf;
  *buf += strcspn(*buf, WS);
  *((*buf)++) = 0;

  *val = atol(ptr);

  return 0;
}

/* Read a string value */
static int _cfg_read_string(char **buf, char **val) {
  char *ptr;

  if (**buf == '"') {
    ptr = ++(*buf);

    while (1) {
      *buf += strcspn(*buf, "\"");
      if (**buf != '"') {
        return -1;
      } else if (*(*buf - 1) == '\\') {
        (*buf)++;
        continue;
      } else {
        break;
      }
    }
  } else {
    ptr = *buf;
    *buf += strcspn(*buf, WS);
  }
  *((*buf)++) = 0;

  *val = x_strdup(ptr);

  return 0;
}