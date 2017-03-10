=========================
List Based Flash Blocking
=========================

List based Flash blocking currently uses six lists.
The lists specify what domains/subdomains Flash is allowed to or denied from loading on.
The domains specified by the lists indicate the domain of the document that the Flash is loaded in, not the domain hosting the Flash content itself.

* Allow List
* Allow Exceptions List
* Deny List
* Deny Exceptions List
* Sub-Document Deny List
* Sub-Document Deny Exceptions List

If a page is on a list and the corresponding "Exceptions List", it is treated as though it is not on that list.

Classification
==============

Documents can be classified as Allow, Deny or Unknown.
Documents with an Allow classification may load Flash normally.
Documents with a Deny classification may not load Flash at all.
A Deny classification overrides an Allow classification.
The Unknown classification is the fall-through classification; it essentially just means that the document did not receive an Allow or Deny classification.
Documents with an Unknown classification will have Flash set to Click To Activate.

If the document is at the top level (its address is in the URL bar), then the Deny List is checked first followed by the Allow List to determine its classification.

If the document is not at the top level, it will receive a Deny classification if the classification of the parent document is Deny or if the document is on the Deny List.
It will also receive a Deny classification if the sub-document is not same-origin and the document is on the Sub-Document Deny List.
If the document did not receive a Deny classification, it can receive an Allow classification if it is on the Allow List or if the parent document received an Allow classification.

If for any reason, the document has a null principal, it will receive a Deny classification.
Some examples of documents that would have a null principal are:

* Data URIs <https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/Data_URIs> loaded directly from the URL bar. Data URIs loaded by a page should inherit the loading page's permissions.
* URIs that are rendered with the JSON viewer
