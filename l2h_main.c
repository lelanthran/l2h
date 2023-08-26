// vim: set ts=3 sw=3 colorcolumn=100 et

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
};

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
   return ret;
}

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

// Returns -1 for error, 0 for EOF and 1 for token
static int token_read (struct token_t **dst,
                       const char *input, size_t input_len, size_t *index)
{
   int c;
   while ((c = getnextchar (input, input_len, index)) != EOF) {

      // Ignore spaces
      if (isspace (c)) {
         continue;
      }

      // Handle the open/close parenthesis cases
      if (c == '(') {
         *dst = token_new (token_OPEN_PAREN, &input[(*index) - 1], 1);
         return 1;
      }

      if (c == ')') {
         *dst = token_new (token_CLOSE_PAREN, &input[(*index) - 1], 1);
         return 1;
      }

      if (c == ':') {
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
                  return -1;
               }
            }
         }
         *dst = token_new (token_ATTR, &input[start], (*index) - start);
         return 1;
      }


      // Maybe at some point in the future we use a static LUT for this.
      if (isalpha (c) || ispunct (c)) {
         size_t start = (*index) - 1;
         while ((c = getnextchar (input, input_len, index)) != EOF) {
            if (isspace (c) || c == '(' || c == ')') {
               (*index)--;
               break;
            }
         }
         *dst = token_new (token_SYMBOL, &input[start], (*index) - start);
         return 1;
      }

      // Nothing matches?
      return -1;
   }

   // Reached EOF
   return 0;
}




/* ********************************************************
 * struct node_t
 */
enum node_type_t {
   node_UNKNOWN = 0,
   node_SYMBOL,
   node_LIST,
};

static const char *node_type_text (enum node_type_t type)
{
   static const struct {
      enum node_type_t type;
      const char *text;
   } arr[] = {
      { node_UNKNOWN,   "node_UNKNOWN" },
      { node_SYMBOL,    "node_SYMBOL"  },
      { node_LIST,      "node_LIST"    },
   };
   static const size_t arr_len = sizeof arr / sizeof arr[0];

   for (size_t i=0; i<arr_len; i++) {
      if (arr[i].type == type) {
         return arr[i].text;
      }
   }

   return arr[0].text;
}

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
   for (size_t i=0; i<(ilevel * 3); i++) {
      fputc (' ', outf);
   }
}


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

   switch (node->type) {
      case node_SYMBOL:
         if (node_prev_type (node) != node_SYMBOL) {
            fputc ('\n', outf);
            print_indent (indent, outf);
         }
         fprintf (outf, "%s ", node->value);
         if (node_is_final (node)) {
            fprintf (outf, "\n");
         }

         break;

      case node_LIST:
         fputc ('\n', outf);
         print_indent (indent, outf);
         const char *attrs = node->attrs ? node->attrs : "";
         const char *delim = node->attrs ? " " : "";
         fprintf (outf, "<%s%s%s>", node->value, delim, attrs);
         for (size_t i=0; i<node->nchildren; i++) {
            node_emit_html (node->children[i], indent + 1, outf);
         }
         print_indent (indent, outf);
         fprintf (outf, "</%s>\n", node->value);
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

#define EXT       (".lisp")
#define EXT_LEN   (strlen (EXT))

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

   if (!ifname) {
      fprintf (stderr, "%s: NULL passed for input filename\n", ifname);
      goto cleanup;
   }

   if (!(ofname = strdup (ifname))) {
      fprintf (stderr, "%s: OOM error allocating output filename\n", ifname);
      goto cleanup;
   }


   if ((strcmp (ifname, "-")) == 0) {
      ofname = strdup ("-");
   } else {
      tmp = strstr (ofname, EXT);
      if (!tmp) {
         fprintf (stderr, "%s: Input filename missing [%s]\n", ifname, EXT);
         goto cleanup;
      }
       *tmp = 0;
   }

   if ((memcmp (&ifname[strlen(ifname) - strlen(EXT)], EXT, EXT_LEN)) != 0) {
      fprintf (stderr, "%s: Input filename must end in [%s]\n", ifname, EXT);
      goto cleanup;
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
      fprintf (stderr, "%s: Parsed all input\n", ifname);
   }
   if (rc > 0) {
      fprintf (stderr, "%s: Unparsed input still in buffer\n", ifname);
      goto cleanup;
   }

   node_emit_html(root, 0, outf);

   ret = EXIT_SUCCESS;
cleanup:
   if (inf && (strcmp (ifname, "-") != 0)) {
      fprintf (stderr, "%s: closed\n", ifname);
      fclose (inf);
   }
   if (outf && (strcmp (ofname, "-") != 0)) {
      fprintf (stderr, "%s: closed\n", ofname);
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
   (void)ifname;
   (void)recurse;
   return false;
}


int main (int argc, char **argv)
{
   int ret = EXIT_FAILURE;
   bool flag_process_dir = false;
   bool flag_recurse = false;
   char **paths = NULL;
   size_t npaths = 0;
   size_t errcount = 0;

   (void)argc;

   for (size_t i=1; argv[i]; i++) {
      if (argv[i][0] == '-') {
         if ((memcmp (argv[i], "-r", 3))==0) {
            flag_recurse = true;
            continue;
         }
         if ((memcmp (argv[i], "-d", 3))==0) {
            flag_process_dir = true;
            continue;
         }
         fprintf (stderr, "Unrecognised flag [%s]\n", argv[i]);
         errcount++;
      } else {
         char **tmp = realloc (paths, (npaths + 2) * (sizeof (*tmp)));
         if (!tmp) {
            fprintf (stderr, "OOM error allocating paths (%zu encountered), aborting\n", npaths);
            goto cleanup;
         }
         tmp[npaths++] = argv[i];
         tmp[npaths++] = NULL;
         paths = tmp;
      }
   }

   if (!paths) {
      fprintf (stderr, "No pathnames specified, aborting\n");
      errcount++;
   }

   if (errcount) {
      fprintf (stderr, "Errors in invocation (%zu), aborting\n", errcount);
      goto cleanup;
   }

   errcount = 0;

   if (!flag_process_dir) {
      for (size_t i=0; paths[i]; i++) {
         if ((process_file(paths[i])) != EXIT_SUCCESS) {
            fprintf (stderr, "Error processing [%s]\n", paths[i]);
            errcount++;
         }
      }
   }

   if (flag_process_dir) {
      for (size_t i=0; paths[i]; i++) {
         if ((process_dir(paths[i], flag_recurse)) != EXIT_SUCCESS) {
            fprintf (stderr, "Error processing [%s]\n", paths[i]);
            errcount++;
         }
      }
   }

   ret = errcount;

cleanup:
   free (paths);
   fprintf (stderr, "Exit-code: %i\n", ret);
   return ret;
}

// returns 0 for EOF, -1 for error and 1 for success
static int parser (struct node_t *parent,
                   const char *input, size_t input_len, size_t *index)
{
   struct token_t *tok;
   int rc;
   static char error_context[81];

   while ((rc = token_read (&tok, input, input_len, index)) > 0) {
      switch (tok->type) {
         case token_OPEN_PAREN:
            token_del (tok);
            if ((rc = token_read (&tok, input, input_len, index)) < 1) {
               break;
            }

            struct node_t *root = node_new (parent, node_LIST, tok->text);
            if (!root) {
               fprintf (stderr, "OOM error constructing root node\n");
               token_del (tok);
               return -1;
            }
            rc = parser (root, input, input_len, index);
            break;

         case token_CLOSE_PAREN:
            token_del (tok);
            return 1;

         case token_SYMBOL:
            if (!(node_new (parent, node_SYMBOL, tok->text))) {
               fprintf (stderr, "Failed to create node\n");
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
   if (rc==0) {
      fprintf (stderr, "Reached end of file\n");
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
   if (rc == 0) {
      fprintf (stderr, "Parsing complete\n");
   }
   if (rc == 1) {
      fprintf (stderr, "Unexpected end of parsing\n");
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



