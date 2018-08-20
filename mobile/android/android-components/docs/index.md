---
layout: page
---

# Overview

**Mozilla Android Components** is a collection of independent, reusable Android library components to make it easier to build browsers and browser-like applications.

# Motivation

* 🚀 **Accelerate development**: Our components are customizable building blocks that are individually adoptable but built to work together.

* 📉 **Reduce maintenance overhead**: Building a browser is a complex task. With the use of our components you get a solid foundation to build on top of.

# Updates

<div class="blog-index">  
{% for post in site.posts limit:5 %}
  {% assign post = post  %}
  {% assign content = post.content %}
  {% include post_detail.html %}
{% endfor %}
</div>
