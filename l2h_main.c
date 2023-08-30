// vim: set ts=3 sw=3 colorcolumn=100 et

// I compile with:
//    gcc -c -W -Wall -Wextra -ggdb l2h_main.c -o l2h

/* ****************************************************************************
 *
 * BSD 2-Clause License
 *
 * Copyright (c) 2023, Lelanthran Manickum
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * **************************************************************************** */


/* ********************************************************
 * This is such a small project, everything can fit into a
 * single file.
 *
 * At some point, maybe I will want to break the reader or
 * parser projects into a separate file purely so that a
 * .so can be produced so that the programmers using other
 * languages (Java, Python, etc) can use the library.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>


/* ********************************************************
 * The globals.
 */

#define VERSION      ("0.0.5")
#define FPRINTF(x,...)  if (flag_verbose) fprintf (stderr, __VA_ARGS__)

static bool flag_verbose = false;

static int getnextchar (const char *input, size_t input_len, size_t *index)
{
   if (*index > input_len)
      return EOF;

   if (input[*index] == 0)
      return EOF;

   int ret = input[*index];
   (*index)++;

   return ret;
}

/* ********************************************************
 * struct token_t
 */

enum token_type_t {
   token_UNKNOWN = 0,
   token_OPEN_PAREN,
   token_CLOSE_PAREN,
   token_SYMBOL,
   token_ATTR,
   token_WHITESPACE,
   token_NEWLINE,
};

#if 0
static const char *token_type_text (enum token_type_t type) {
   static const struct {
      enum token_type_t type;
      const char *text;
   } arr[] = {
      { token_UNKNOWN,     "token_UNKNOWN"      },
      { token_OPEN_PAREN,  "token_OPEN_PAREN"   },
      { token_CLOSE_PAREN, "token_CLOSE_PAREN"  },
      { token_SYMBOL,      "token_SYMBOL"       },
      { token_ATTR,        "token_ATTR"         },
   };
   static const size_t arr_len = sizeof arr/sizeof arr[0];

   for (size_t i=0; i<arr_len; i++) {
      if (type == arr[i].type) {
         return arr[i].text;
      }
   }
   return arr[0].text;
}
#endif


struct token_t {
   enum token_type_t type;
   char *text;
   size_t text_len;
};

static void token_del (struct token_t *token)
{
   if (!token)
      return;

   free (token->text);
   free (token);
}

static struct token_t *token_new (enum token_type_t type, const char *val, size_t val_len)
{
   struct token_t *ret = calloc (1, sizeof *ret);
   if (!ret) {
      fprintf (stderr, "OOM error allocating token\n");
      return NULL;
   }

   if (!(ret->text = malloc (val_len + 2))) {
      fprintf (stderr, "OOM error allocating token value\n");
      token_del (ret);
      return NULL;
   }

   ret->type = type;
   ret->text_len = val_len;
   memcpy (ret->text, val, val_len);
   ret->text[val_len] = 0;
   for (size_t i=0; ret->text[i]; i++) {
      if (ret->text[i] == '\\') {
         memmove (&ret->text[i], &ret->text[i+1], ret->text_len - i + 1);
         ret->text_len--;
      }
   }

   return ret;
}

#if 0
// Only for diagnostics during development
static void token_dump (struct token_t *token, FILE *outf)
{
   if (!outf)
      outf = stdout;

   if (!token) {
      fprintf (outf, "(null token)\n");
      return;
   }

   fprintf (outf, "token: [%19s...%s]\n", token_type_text (token->type), token->text);
}
#endif

enum reader_action_t {
   reader_ERROR = -1,
   reader_EOF = 0,
   reader_TOKEN = 1,
   reader_CONTINUE = 2,
};

#define READER_STATE_ATTRS    (1)
static int token_read (struct token_t **dst, uint64_t *state,
                       const char *input, size_t input_len, size_t *index)
{
   static char errbuf[1024];
   int c;
   while ((c = getnextchar (input, input_len, index)) != EOF) {

      // Return each newline as a token
      if (c == '\n') {
         if ((*state) & READER_STATE_ATTRS) {
            return reader_CONTINUE;
         }
         *dst = token_new (token_NEWLINE, &input[(*index) - 1], 1);
         return reader_TOKEN;
      }

      // Compress spaces that are not newlines
      if ((c != '\n') && isspace (c)) {
         if ((*state) & READER_STATE_ATTRS) {
            return reader_CONTINUE;
         }
         size_t start = (*index);
         while ((c = getnextchar (input, input_len, index)) != EOF &&
                (c != '\n') &&
                isspace (c)) {
            ;
         }
         (*index)--;
         *dst = token_new (token_WHITESPACE, &input[start], (*index) - start);
         return reader_TOKEN;
      }

      // Handle the open/close parenthesis cases
      if (c == '(') {
         *state = (*state) & ~READER_STATE_ATTRS;
         *dst = token_new (token_OPEN_PAREN, &input[(*index) - 1], 1);
         return reader_TOKEN;
      }

      if (c == ')') {
         *state = (*state) & ~READER_STATE_ATTRS;
         *dst = token_new (token_CLOSE_PAREN, &input[(*index) - 1], 1);
         return reader_TOKEN;
      }

      // Handle element attributes.
      if (c == ':') {
         *state = (*state) | READER_STATE_ATTRS;
         size_t start = *index;
         while ((c = getnextchar (input, input_len, index)) != EOF) {
            if (isspace (c) || c == '(' || c == ')' || c == '=') {
               (*index)--;
               break;
            }
         }
         if (c == '=') {
            getnextchar (input, input_len, index); // swallow the '='
            c = getnextchar (input, input_len, index);
            if (c == '"' || c == '\'') {
               int quote = c;
               while ((c = getnextchar (input, input_len, index)) != EOF) {
                  if (c == quote) {
                     break;
                  }
               }
               if (c != quote) {
                  *index = start;
                  snprintf (errbuf, sizeof errbuf - 1, "%s", &input[*index]);
                  fprintf (stderr, "Unmatched quote [%c] at\n%s\n", c, errbuf);
                  return reader_ERROR;
               }
            }
         }
         *dst = token_new (token_ATTR, &input[start], (*index) - start);
         return reader_TOKEN;
      }


      // Maybe at some point in the future we use a static LUT for this.
      if (isalpha (c) || ispunct (c)) {
         *state = (*state) & ~READER_STATE_ATTRS;
         size_t start = (*index) - 1;
         if (c == '\\')
            c = getnextchar (input, input_len, index);
         while ((c = getnextchar (input, input_len, index)) != EOF) {
            if (c == '\\') {
               c = getnextchar (input, input_len, index);
               continue;
            }
            if (isspace (c) || c == '(' || c == ')') {
               (*index)--;
               break;
            }
         }
         *dst = token_new (token_SYMBOL, &input[start], (*index) - start);
         return reader_TOKEN;
      }

      // Nothing matches?
      snprintf (errbuf, sizeof errbuf - 1, "%s", &input[*index]);
      fprintf (stderr, "No token matches succeeded at:\n");
      fprintf (stderr, "--------\n%s\n---------\n", errbuf);
      return reader_ERROR;
   }

   // Reached EOF
   return reader_EOF;
}




/* ********************************************************
 * struct node_t
 */
enum node_type_t {
   node_UNKNOWN = 0,
   node_SYMBOL,
   node_LIST,
   node_WHITESPACE,
   node_NEWLINE,
};

#if 0
static const char *node_type_text (enum node_type_t type)
{
   static const struct {
      enum node_type_t type;
      const char *text;
   } arr[] = {
      { node_UNKNOWN,      "node_UNKNOWN" },
      { node_SYMBOL,       "node_SYMBOL"  },
      { node_LIST,         "node_LIST"    },
      { node_WHITESPACE,   "node_WHITESPACE"    },
      { node_NEWLINE,      "node_NEWLINE"    },
   };
   static const size_t arr_len = sizeof arr / sizeof arr[0];

   for (size_t i=0; i<arr_len; i++) {
      if (arr[i].type == type) {
         return arr[i].text;
      }
   }

   return arr[0].text;
}
#endif

struct node_t {
   enum node_type_t type;
   char *value;
   char *attrs;
   size_t attrs_len;
   struct node_t **children;
   size_t nchildren;
   struct node_t *parent;
};

static void node_del (struct node_t *node)
{
   if (!node)
      return;

   for (size_t i=0; i<node->nchildren; i++) {
      node_del (node->children[i]);
   }

   free (node->value);
   free (node->attrs);
   free (node->children);
   free (node);
}

static struct node_t *node_new (struct node_t *parent,
                                enum node_type_t type, const char *value)
{
   struct node_t *ret = calloc (1, sizeof *ret);
   if (!ret) {
      fprintf (stderr, "OOM error allocating node\n");
      return NULL;
   }

   ret->parent = parent;
   ret->type = type;
   if (!(ret->value = strdup (value))) {
      fprintf (stderr, "OOM error allocating node->value\n");
      node_del (ret);
      return NULL;
   }

   if (parent) {
      struct node_t **tmp = realloc (parent->children, (parent->nchildren + 1) * (sizeof *tmp));
      if (!tmp) {
         fprintf (stderr, "OOM error appending child to parent\n");
         node_del (ret);
         return NULL;
      }
      tmp[parent->nchildren++] = ret;
      parent->children = tmp;
   }

   return ret;
}

static bool node_add_attr (struct node_t *node, const char *attr)
{
   if (!node)
      return false;

   size_t attr_len = strlen (attr);
   char *tmp = realloc (node->attrs, node->attrs_len + attr_len + 2);
   if (!tmp) {
      fprintf (stderr, "OOM error allocating node->attrs\n");
      return false;
   }

   tmp[node->attrs_len] = ' ';
   strcpy (&tmp[node->attrs_len+1], attr);
   node->attrs = tmp;
   node->attrs_len += attr_len + 1;
   return true;
}

static void print_indent(size_t ilevel, FILE *outf)
{
   for (size_t i=0; i<ilevel; i++) {
      fputc ('\t', outf);
   }
}


#if 0
static void node_dump (struct node_t *node, size_t indent)
{
   if (!node)
      return;

   print_indent (indent, stdout);
   printf ("[node: %s...%s] %zu\n", node_type_text (node->type), node->value, node->nchildren);

   print_indent (indent, stdout);
   printf ("attributes=[%s]\n", node->attrs);
   for (size_t i=0; i<node->nchildren; i++) {
      node_dump (node->children[i], indent + 1);
   }
}
#endif

static size_t node_get_index (const struct node_t *node)
{
   if (node && node->parent) {
      for (size_t i=0; i<node->parent->nchildren; i++) {
         if (node->parent->children[i] == node) {
            return i;
         }
      }

   }
   return (size_t)-1;
}

static enum node_type_t node_prev_type (const struct node_t *node)
{
   size_t node_idx = node_get_index (node);

   if (!node->parent || node_idx == (size_t)-1 || node_idx == 0) {
      return node_UNKNOWN;
   }

   return node->parent->children[node_idx -1]->type;
}

static bool node_is_final (const struct node_t *node)
{
   size_t node_idx = node_get_index (node);

   if (node->parent && node_idx != (size_t)-1 && node_idx == (node->parent->nchildren - 1)) {
      return true;
   }

   return false;
}

static void node_emit_html (const struct node_t *node, size_t indent, FILE *outf)
{
   if (!node)
      return;

   const char *attrs = NULL;
   const char *delim = NULL;
   switch (node->type) {
      case node_NEWLINE:
         fprintf (outf, "\n");
         print_indent (indent, outf);
         break;

      case node_WHITESPACE:
         fprintf (outf, " ");
         break;

      case node_SYMBOL:
         fprintf (outf, "%s", node->value);
         break;

      case node_LIST:
         attrs = node->attrs ? node->attrs : "";
         delim = node->attrs ? " " : "";
         fprintf (outf, "<%s%s%s>", node->value, delim, attrs);
         for (size_t i=0; i<node->nchildren; i++) {
            node_emit_html (node->children[i], indent + 1, outf);
         }
         fprintf (outf, "</%s>", node->value);
         break;

      case node_UNKNOWN:
      default:
         fprintf (stderr, "Unknown node type %i\n", node->type);
         break;
   }

}



static bool collect_input (char **dst, size_t *dst_len, const char *line);
static int parse (struct node_t **dst,
                  const char *input, size_t input_len, size_t *index);


/* ********************************************************
 * Main Functions
 */

static int process_file (const char *ifname)
{
   static char line[1024 * 1024];
   size_t nlines = 0;
   char *input = NULL;
   size_t input_len = 0;
   struct node_t *root = NULL;

   int ret = EXIT_FAILURE;
   FILE *inf = NULL, *outf = NULL;
   char *ofname = NULL;
   char *tmp = NULL;

   static const char *fext = ".html.lisp";

   if (!ifname) {
      fprintf (stderr, "%s: NULL passed for input filename\n", ifname);
      goto cleanup;
   }

   if (!(ofname = strdup (ifname))) {
      fprintf (stderr, "%s: OOM error allocating output filename\n", ifname);
      goto cleanup;
   }

   // TODO: Fix this.
   // If the input filename is "-", then set the output filename to "-"
   // otherwise, set the output filename to the input filename, stripping
   // off everything past ".html.lisp". This is non-ideal for files that
   // have two instances of that string in their name, hence the TODO
   if ((strcmp (ifname, "-")) == 0) {
      free (ofname);
      ofname = strdup ("-");
   } else {
      tmp = strstr (ofname, fext);
      if (!tmp) {
         fprintf (stderr, "%s: Input filename missing [%s]\n", ifname, fext);
         goto cleanup;
      }
      tmp = strstr (tmp, ".lisp"); // cannot fail, so omit the check
       *tmp = 0;
   }

   if ((memcmp (ifname, "-", 2)) == 0) {
      inf = stdin;
   } else {
      if (!(inf = fopen (ifname, "r"))) {
         fprintf (stderr, "%s: opened\n", ifname);
         fprintf (stderr, "%s: Failed to open [%s] for reading: %m\n", ifname, ifname);
         goto cleanup;
      }
   }

   if ((memcmp (ofname, "-", 2)) == 0) {
      outf = stdout;
   } else {
      if (!(outf = fopen (ofname, "w"))) {
         fprintf (stderr, "%s: opened\n", ofname);
         fprintf (stderr, "%s: Failed to open [%s] for writing: %m\n", ifname, ofname);
         goto cleanup;
      }
   }

   while (!(feof (inf)) && !(ferror (inf)) && (fgets (line, sizeof line -1, inf))) {
      nlines++;
      if (!(collect_input (&input, &input_len, line))) {
         fprintf (stderr, "%s: OOM error reading line %zu\n", ifname, nlines);
         goto cleanup;
      }
   }

   size_t index = 0;
   input_len = strlen (input);
   int rc = parse (&root, input, input_len,  &index);
   if (rc < 0) {
      fprintf (stderr, "%s: Failed to parse input, aborting\n", ifname);
      goto cleanup;
   }
   if (rc == 0) {
      FPRINTF (stderr, "%s: complete\n", ifname);
   }
   if (rc > 0) {
      fprintf (stderr, "%s: Unparsed input still in buffer\n", ifname);
      goto cleanup;
   }

   for (size_t i=0; i<root->nchildren; i++) {
      node_emit_html(root->children[i], 0, outf);
   }

   ret = EXIT_SUCCESS;
cleanup:
   if (inf && (strcmp (ifname, "-") != 0)) {
      fclose (inf);
   }
   if (outf && (strcmp (ofname, "-") != 0)) {
      fclose (outf);
   }

   free (ofname);

   node_del (root);
   if (!nlines) {
      fprintf (stderr, "%s: No input provided. See the documentation for help\n", ifname);
      goto cleanup;
   }

   free (input);
   return ret;
}

static int process_dir (const char *ifname, bool recurse)
{
   int errcount = 1;
   char *pwd = getcwd (NULL, 0); // gnu extension, allocates the returned value
   struct dirent *de = NULL;
   DIR *dirp = NULL;

   if ((chdir (ifname)) != 0) {
      fprintf (stderr, "Failed to switch to directory [%s]: %m\n", ifname);
      goto cleanup;
   }

   FPRINTF (stderr, "Changed to directory [%s]\n", ifname);

   if (!pwd) {
      fprintf (stderr, "Failed to switch to directory [%s]: %m\n", ifname);
      goto cleanup;
   }

   if (!(dirp = opendir ("."))) {
      fprintf (stderr, "Failed to directory [%s] for reading: %m\n", ifname);
      goto cleanup;
   }

   errcount = 0;
   errno = 0;

   while ((de = readdir (dirp)) != NULL) {
      if (errno) {
         fprintf (stderr, "Failed to read directory [%s]: %m\n", ifname);
         errcount++;
         goto cleanup;
      }
      if (de->d_name[0] == '.') {
         continue;
      }
      if (recurse && de->d_type == DT_DIR) {
         errcount += process_dir (de->d_name, recurse) == 0 ? 0 : 1;
         continue;
      }
      // TODO: Still handling file-checking wrong. This will have some
      // false positives. Check the other TODO about why this is broken
      if ((strstr (de->d_name, ".html.lisp")) != NULL) {
         errcount += process_file(de->d_name) == 0 ? 0 : 1;
      }
      errno = 0;
   }


cleanup:
   if ((chdir (pwd)) != 0) {
      fprintf (stderr, "Failed to return to previous directory: %m\n");
   }

   FPRINTF (stderr, "Left directory [%s]\n", ifname);

   free (pwd);
   if (dirp) {
      closedir (dirp);
   }
   return errcount;
}

static void print_help_msg (void)
{
   static const char *msg[] = {
"Lisp2Html: Convert lisp-ish s-expressions to HTML tag trees",
"Usage:",
"  l2h [options] PATH1 PATH2 ... PATHn",
"",
"  Each path must be a filename of the form '*.html.lisp' or a directory",
"name. When PATH is a filename, the file is converted and the results",
"are written to '*.html' (i.e. the '.lisp' is removed from the output",
"filename). When PATH is a directory it is scanned for all files matching",
"the pattern '*.html.lisp' and each file is converted, with the results",
"stored in '*.html'",
"",
"  Unless the option '-r' or '--recurse' is specified, directories are",
"not recursively processed. If the option '-s' or '--stdio' is specified",
"then input is read from stdin and written to stdout. All pathnames are",
"ignored when '-s' or '--stdio' is specified.",
"",
"  The following options are recognised. Unrecognised options produce an",
"error message without any processing of files or data.",
"",
"-r | --recurse     Recursively process any directories specified",
"-s | --stdio       Read input from stdin and write the output to stdout",
"-v | --verbose     Produce extra informational messages",
"-V | --version     Print the program version, then continue as normal",
"-h | --help        Display this message and exit",
"",
"",
   };

   for (size_t i=0; i<sizeof msg / sizeof msg[0]; i++) {
      printf ("%s\n", msg[i]);
   }
}

int main (int argc, char **argv)
{
   int ret = EXIT_FAILURE;
   bool flag_recurse = false,
         flag_version = false,
         flag_help = false,
         flag_stdio = false;
   char **paths = NULL;
   size_t npaths = 0;
   size_t errcount = 0;

   (void)argc;

   for (size_t i=1; argv[i]; i++) {
      if (argv[i][0] == '-') {
         if ((strcmp (argv[i], "-r"))==0 || (strcmp (argv[i], "--recurse"))==0) {
            flag_recurse = true;
            continue;
         }
         if ((strcmp (argv[i], "-v"))==0 || (strcmp (argv[i], "--verbose"))==0) {
            flag_verbose = true;
            continue;
         }
         if ((strcmp (argv[i], "-V"))==0 || (strcmp (argv[i], "--version"))==0) {
            flag_version = true;
            continue;
         }
         if ((strcmp (argv[i], "-h"))==0 || (strcmp (argv[i], "--help"))==0) {
            flag_help = true;
            continue;
         }
         if ((strcmp (argv[i], "-s"))==0 || (strcmp (argv[i], "--stdio"))==0) {
            flag_stdio = true;
            continue;
         }
         fprintf (stderr, "Unrecognised flag [%s]. Try --help\n", argv[i]);
         errcount++;
      } else {
         char **tmp = realloc (paths, (npaths + 2) * (sizeof (*tmp)));
         if (!tmp) {
            fprintf (stderr, "OOM error allocating paths (%zu encountered), aborting\n", npaths);
            goto cleanup;
         }
         tmp[npaths++] = argv[i];
         tmp[npaths] = NULL;
         paths = tmp;
      }
   }

   if (flag_version) {
      printf ("l2h v%s (Copyright Lelanthran Manickum 2023)\n\n", VERSION);
      printf ("  This program is licensed under the BSD 2-Clause license.\n");
      printf ("  If you would like the source code or the license to this\n");
      printf ("  program, please visit\n\n");
      printf ("            https://github.com/lelanthran/l2h\n\n");
   }

   if (flag_help) {
      print_help_msg ();
      free (paths);
      return 0;
   }


   if (!paths && !flag_stdio && !flag_recurse) {
      fprintf (stderr, "No pathnames specified, aborting\n");
      errcount++;
   }

   if (errcount) {
      fprintf (stderr, "Errors in invocation (%zu), aborting\n", errcount);
      goto cleanup;
   }

   errcount = 0;

   for (size_t i=0; !flag_stdio && paths[i]; i++) {
      struct stat sb;
      if ((stat (paths[i], &sb)) != 0) {
         fprintf (stderr, "Failed to stat [%s]: %m\n", paths[i]);
         errcount++;
         continue;
      }
      if (S_ISDIR (sb.st_mode)) {
         if ((process_dir (paths[i], flag_recurse)) != EXIT_SUCCESS) {
            fprintf (stderr, "Error processing directory [%s]: %m\n", paths[i]);
            errcount++;
            continue;
         }
      } else {
         if ((process_file (paths[i])) != EXIT_SUCCESS) {
            fprintf (stderr, "Error processing [%s]\n", paths[i]);
            errcount++;
            continue;
         }
      }
   }

   if (flag_stdio) {
      errcount += process_file("-") == EXIT_SUCCESS ? 0 : 1;
   }

   ret = errcount;

cleanup:
   free (paths);
   FPRINTF (stderr, "Exit-code: %i\n", ret);
   return ret;
}

// returns 0 for EOF, -1 for error and 1 for success
static int parser (struct node_t *parent,
                   const char *input, size_t input_len, size_t *index)
{
   struct token_t *tok;
   enum reader_action_t rc;
   static char error_context[81];
   uint64_t state = 0;

   while (1) {
      rc = token_read (&tok, &state, input, input_len, index);
      if (rc == reader_CONTINUE) {
         continue;
      }
      if (rc == reader_EOF || rc == reader_ERROR) {
         break;
      }

      switch (tok->type) {
         case token_OPEN_PAREN:
            token_del (tok);
            rc = token_read (&tok, &state, input, input_len, index);
            if (rc == reader_CONTINUE) {
               continue;
            }
            if (rc == reader_EOF || rc == reader_ERROR) {
               break;
            }

            struct node_t *root = node_new (parent, node_LIST, tok->text);
            if (!root) {
               fprintf (stderr, "OOM error constructing root node\n");
               token_del (tok);
               return -1;
            }
            // Hack to swallow whitespace after any symbol, but preserve
            // newlines as-is. This lets us emit things like "A(tag B)C"
            // (note, no spaces on either side of the tags) while ensuring
            // that "(tag\ncontent)" results in "<tag>\ncontent</tag>".
            //
            // This is because when a user indicates a newline after a tag,
            // we should respect that in the output, but any spaces after
            // a tagname needs to be removed as the user doesn't want the
            // input "A(tag B)C" to be turned into "A <tag> B </tag> C".
            int c;
            while ((c = getnextchar (input, input_len, index))!=EOF) {
               if ((c == '\n') || !(isspace (c))) {
                  (*index)--;
                  break;
               }
            }

            rc = parser (root, input, input_len, index);
            break;

         case token_CLOSE_PAREN:
            token_del (tok);
            return 1;

         case token_SYMBOL:
            if (!(node_new (parent, node_SYMBOL, tok->text))) {
               fprintf (stderr, "Failed to create symbol node\n");
               token_del (tok);
               return -1;
            }
            break;

         case token_ATTR:
            if (!(node_add_attr(parent, tok->text))) {
               fprintf (stderr, "Failed to add attribute\n");
               token_del (tok);
               return -1;
            }
            break;

         case token_WHITESPACE:
            if (!(node_new (parent, node_WHITESPACE, tok->text))) {
               fprintf (stderr, "Failed to create whitespace node\n");
               token_del (tok);
               return -1;
            }
            break;

         case token_NEWLINE:
            if (!(node_new (parent, node_NEWLINE, tok->text))) {
               fprintf (stderr, "Failed to create newline node\n");
               token_del (tok);
               return -1;
            }
            break;

         case token_UNKNOWN:
         default:
            fprintf (stderr, "Unknown token [%s]\n", tok->text);
            token_del (tok);
            return -1;

      }

      token_del (tok);
   }
   if (rc < 0) {
      snprintf (error_context, sizeof error_context - 1, "%s", &input[(*index)-1]);
      fprintf (stderr, "Encountered an error while parsing near:\n%s\n", error_context);
   }

   return rc;
}

static int parse (struct node_t **dst,
                  const char *input, size_t input_len, size_t *index)
{
   struct node_t *root = node_new (NULL, node_LIST, "root");
   if (!root) {
      fprintf (stderr, "OOM error constructing root node\n");
      return -1;
   }

   int rc;
   if ((rc = parser (root, input, input_len, index)) < 0) {
      fprintf (stderr, "Failed to parse\n");
   }
   if (rc == 1) {
      fprintf (stderr, "Unexpected end of parsing\n");
      fprintf (stderr, "Remained of buffer follows:\n");
      fprintf (stderr, "======================\n%s======================\n",
               &input[*index]);
   }


   *dst = root;

   return rc;
}

static bool collect_input (char **dst, size_t *dst_len, const char *line)
{
   size_t line_len = strlen (line);
   char *tmp = realloc (*dst, *dst_len + line_len + 1);
   if (!tmp) {
      return false;
   }

   strcpy (&tmp[*dst_len], line);

   *dst = tmp;
   *dst_len += line_len;

   return true;
}

