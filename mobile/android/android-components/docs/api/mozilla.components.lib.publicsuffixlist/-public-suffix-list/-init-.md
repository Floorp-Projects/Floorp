[android-components](../../index.md) / [mozilla.components.lib.publicsuffixlist](../index.md) / [PublicSuffixList](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`PublicSuffixList(context: <ERROR CLASS>, dispatcher: CoroutineDispatcher = Dispatchers.IO, scope: CoroutineScope = CoroutineScope(dispatcher))`

API for reading and accessing the public suffix list.

A "public suffix" is one under which Internet users can (or historically could) directly register names. Some
 examples of public suffixes are .com, .co.uk and pvt.k12.ma.us. The Public Suffix List is a list of all known
 public suffixes.

Note that this implementation applies the rules of the public suffix list only and does not validate domains.

https://publicsuffix.org/
https://github.com/publicsuffix/list

