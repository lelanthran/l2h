# l2h

## Convert **Non**Lisp S-expressions to HTML.

1. Write S-expressions,
2. Run `l2h`,
3. Use generated HTML file(s).

Here's a small example of `s-exp` input and `HTML` output:

<ins>Input</ins>
```elisp
(html
  (body
    (h1 :disabled Hello world!)
    (div :class="alert"
      So long, and (b thanks) for all the (em fish))))
```

<ins>Output</ins>
```html
<html>
  <body>
    <h1  disabled>Hello world!</h1>
    <div  class="alert">So long, and <b>thanks</b> for all the <em>fish</em></div></body></html>
```
</div>

> [!NOTE]
> *The output is actually tab-indented; in the interests of brevity I
> have substituted 2x spaces for each tab.*

## Language Rules
There are only two main rules to remember, both of which are very simple[^1]:

1. The first symbol of each s-expression is the HTML tag.
2. Attributes for an HTML tag must be preceded with a `:` character.

There's *one additional rule*:
1. Tagnames starting with a period are all reserved for `l2h` builtin
   functions.

Here's how it looks in practice:

#### The first symbol of each s-expression is the HTML tag.

> <ins>Input</ins>
> ```elisp
>   (MyTag Some random content goes here)
> ```
> <ins>Output</ins>
> ```html
>   <MyTag>Some random content goes here</MyTag>
> ```

#### Attributes are prefixed with a `:`

> <ins>Input</ins>
> ```elisp
>   (MyTag :class="alert" :readonly Some random text)
> ```
> <ins>Output</ins>
> ```html
>   <MyTag  class="alert" readonly>Some random text</MyTag>
> ```

That's pretty much all you need to know[^2] to generate HTML.

## Features
### Newlines
Newlines in the source are respected as far as possible so that the
generated HTML matches the visual structure of the source s-expression.

> <ins>Input</ins>
> ```elisp
> (html
>   (body (h1 :disabled Hello world!)))
> ```
>
> <ins>Output</ins>
> ```html
> <html>
>   <body><h1  disabled>Hello world!</h1></body></html>
> ```

### Spaces
Spaces have significance to ensure that results like `He<b>ll</b>o` and
`Good <b>Morning</b> world` come out as expected. In particular:
- Spaces are significant prior to a starting tag.
- Spaces are significant prior to an ending tag.
- Spaces are significant after an ending tag.

> <ins>Input</ins>
> ```
> He(b ll)o
> Good (b Morning) World
> Good (b Morning )World
> ```
> <ins>Output</ins>
> ```html
> He<b>ll</b>o
> Good <b>Morning</b> World
> Good <b>Morning </b>World
> ```

### Parentheses in content
You will eventually need to include parentheses in content. To do this use the
special builtin tagname `.` like this:

> <ins>Input</ins>
> ```elisp
>  (p This is how (. an aside parenthetical) should look)
> ```
> <ins>Output</ins>
> ```html
>  <p>This is how (an aside parenthetical) should look</p>
> ```


### Speed
As this is meant to be part of my workflow, speed is one of the more important
criteria, especially complete duration (which includes startup speed). I use
inotify tools to watch a directory for changes and then apply `l2h -r` which
processes all named directories recursively.
In practice, the delay is imperceptible to me.

For most projects, I will be surprised if you notice `l2h` added to your
workflow just by looking at build times.

For large amounts of input content, it can be noticeable. I imagine that when
my filecounts grow that large I'd make some attempt at optimisation.

On my VirtualBox instance (4 cores, 6GB RAM), the [speed test
script](./speed-test.sh) produced the following data at different input data
sizes (when processing recursively).

| Files|Dirs|Size|Create (secs)|Convert (secs)|
| :--- | :--- | :--- | :--- | :---|
| 100|111|14M|0.29|0.99|
| 400|421|55M|1.16|3.88|
| 900|931|123M|2.60|9.37|
| 1600|1641|219M|4.72|16.08|
| 2500|2551|341M|7.29|25.42|
| 3600|3661|491M|10.59|36.87|
| 4900|4971|668M|14.17|50.28|
| 6400|6481|873M|19.73|64.00|
| 8100|8191|1.1G|26.55|82.88|


## BUGS

#### The ':' character in content is handled poorly.

> The `:` character must be escaped whenever it occurs as the first piece of
> content after a tagname, else it will be parsed as an attribute.
>

For example, here is how the bug manifests, and how escaping fixes it.

> <ins>Input</ins>
> ```
> (tag :attr1 :not-attr the :not-attr is content, not tag)
> (tag :attr1 \:not-attr the :not-attr is content, not tag)
> ```
> <ins>Output</ins>
> ```html
> <tag  attr1 not-attr>the :not-attr is content, not tag</tag>
> <tag  attr1>not-attr the :not-attr is content, not tag</tag>
> ```


## Installation
Either grab the pre-compiled package (for Linux/x64 only, for now) or download
the single `./l2h_main.c` file and  compile it (tested with `gcc`, `clang` and
`tcc`).

> [!NOTE]
> While this is Linux-only right now, I'll add Windows support if anyone ever
> asks for it. Same for *BSDs. I dunno about Mac as I don't have one anymore.
> I'm also working on the assumption that there's a github actions runner for
> whatever platform is being requested.

## Help
Feel free to log issues. Starting the program with `l2h --help` prints out all
the options available, and all the flags supported.

```
Lisp2Html: Convert lisp-ish s-expressions to HTML tag trees
Usage:
  l2h [options] PATH1 PATH2 ... PATHn

  Each path must be a filename of the form '*.html.lisp' or a directory
name. When PATH is a filename, the file is converted and the results
are written to '*.html' (i.e. the '.lisp' is removed from the output
filename). When PATH is a directory it is scanned for all files matching
the pattern '*.html.lisp' and each file is converted, with the results
stored in '*.html'

  Unless the option '-r' or '--recurse' is specified, directories are
not recursively processed. If the option '-s' or '--stdio' is specified
then input is read from stdin and written to stdout. All pathnames are
ignored when '-s' or '--stdio' is specified.

  The following options are recognised. Unrecognised options produce an
error message without any processing of files or data.

-r | --recurse     Recursively process any directories specified
-s | --stdio       Read input from stdin and write the output to stdout
-v | --verbose     Produce extra informational messages
-V | --version     Print the program version, then continue as normal
-h | --help        Display this message and exit

```


## Footnotes
[^1]: I was aiming for intuitive. I may have misjudged.

[^2]: Caveats apply, see the [BUGS section](#BUGS).
