# l2h

## Convert Lisp(-ish) S-expressions to HTML.

1. Write S-expressions,
2. Run `l2h`,
3. Use generated HTML file(s).

Here's a small example of `s-exp` input and `HTML` output:

**Input**
```elisp
(html
  (body
    (h1 Hello world!)
    (div :class="alert"
      So long, and (b thanks) for all the (em fish))))
```

**Output**
```html
<html>
  <body>
    <h1  disabled> Hello world!</h1>
    <div  class="alert">
      So long, and <b>thanks</b> for all the <em>fish</em></div></body></html>
```
</div>

> [!Warning]
> *The output is actually tab-indented; in the interests of brevity I
> have substituted 2x spaces for each tab.*

## Language Rules
The rules are very simple[^1]:

#### The first symbol of each s-expression is the html tag.

> [!NOTE]
> ***Input***
> ```
>   (MyTag Some random content goes here)
> ```
> ***Output***
> ```html
>   <MyTag>Some random content goes here</MyTag>
> ```

#### Attributes are prefixed with a `:`

> [!NOTE]
> ***Input***
> ```
>   (MyTag :class="alert" :readonly Some random text)
> ```
> ***Output***
> ```html
>   <MyTag  class="alert" readonly>  Some random text</MyTag>
> ```


## Demo

## Where to get it

## Help

## Footnotes
[^1]: I was aiming for intuitive. It's probable I misjudged.
