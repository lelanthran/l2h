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

static bool collect_input (char **dst, size_t *dst_len, const char *line);
static struct node_t *parse (const char *input, size_t input_len, size_t *index);

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
   token_COLON,
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
      { token_COLON,       "token_COLON"        },
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

   fprintf (outf, "token: [%19s]...%s...]\n", token_type_text (token->type), token->text);
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
      if (isalpha (c)) {
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
   node_STRING,
   node_SYMBOL,
   node_LIST,
};

struct node_t {
   enum node_type_t type;
   char *value;
   char *attrs;
   size_t attrs_len;
   struct node_t **children;
   size_t nchildren;
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
   free (node);
}

static struct node_t *node_new (enum node_type_t type, const char *value)
{
   struct node_t *ret = calloc (1, sizeof *ret);
   if (!ret) {
      fprintf (stderr, "OOM error allocating node\n");
      return NULL;
   }

   ret->type = type;
   if (!(ret->value = strdup (value))) {
      fprintf (stderr, "OOM error allocating node->value\n");
      node_del (ret);
      return NULL;
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
   strcpy (&tmp[node->attrs_len + 1], attr);
   node->attrs = tmp;
   node->attrs_len += attr_len;
   return true;
}

static void print_indent(size_t ilevel)
{
   for (size_t i=0; i<(ilevel * 3); i++) {
      putchar (' ');
   }
}


static void node_dump (struct node_t *node, size_t indent)
{
   if (!node)
      return;

   print_indent(indent); printf ("node: %s\n", node->value);
   print_indent(indent); printf ("      %s\n", node->attrs);
   for (size_t i=0; i<node->nchildren; i++) {
      node_dump (node->children[i], indent + 1);
   }
}


/* ********************************************************
 * Main
 */
int main (int argc, char **argv)
{
   static char line[1024 * 1024];
   size_t nlines = 0;
   int ret = EXIT_FAILURE;
   char *input = NULL;
   size_t input_len = 0;
   struct node_t *root = NULL;

   char *tmp;

   FILE *inf = NULL, *outf = NULL;

   if (argc <= 2) {
      fprintf (stderr, "Need two arguments: <input-file> <output-file>\n");
      return EXIT_FAILURE;
   }

   if (!(inf = fopen (argv[1], "r"))) {
      fprintf (stderr, "Failed to open [%s] for reading: %m\n", argv[1]);
      goto cleanup;
   }

   if (!(outf = fopen (argv[2], "w"))) {
      fprintf (stderr, "Failed to open [%s] for writing: %m\n", argv[2]);
      goto cleanup;
   }

   while (!(feof (inf)) && !(ferror (inf)) && (fgets (line, sizeof line -1, inf))) {
      nlines++;
      if ((tmp = strchr (line, '\n')))
         *tmp = 0;
      if (!(collect_input (&input, &input_len, line))) {
         fprintf (stderr, "OOM error reading line %zu\n", nlines);
         goto cleanup;
      }
   }

   size_t index = 0;
   input_len = strlen (input);
   if (!(root = parse (input, input_len,  &index))) {
      fprintf (stderr, "Failed to parse input, aborting\n");
      goto cleanup;
   }

   node_dump (root, 0);

   ret = EXIT_SUCCESS;

cleanup:
   if (inf) {
      fclose (inf);
   }

   if (outf) {
      fclose (outf);
   }

   node_del (root);
   if (!nlines) {
      fprintf (stderr, "No input provided. See the documentation for help\n");
      goto cleanup;
   }

   free (input);
   return ret;
}

static struct node_t *parse (const char *input, size_t input_len, size_t *index)
{
   struct node_t *root = node_new (node_SYMBOL, "root");
   struct token_t *tok;
   int rc;
   static char error_context[81];
   while ((rc = token_read (&tok, input, input_len, index)) > 0) {
      token_dump (tok, NULL);
      token_del (tok);
   }
   if (rc < 0) {
      snprintf (error_context, sizeof error_context - 1, "%s", &input[(*index)-1]);
      fprintf (stderr, "Encountered an error while parsing near:\n%s\n", error_context);
   }
   if (rc==0) {
      fprintf (stderr, "Reached end of file\n");
   }


   return root;
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



