Orb
===

A simple scheme-like embeddable toy scripting language in C++.

This is a work in progress, and I do not advice anyone to consider
this usable. The sources are of 'hobby hacking' quality where the main
constraint is the really limited time available for execution - concepts used
are fairly lucid but the structure could be improved.

Orb is a quite limited variant of Scheme. The syntax is a bit different
with influences from Clojure. The feature set is not extensive - closures are
supported but there is no tail call optimization. Only charset supported is
US-ASCII.

Dependencies and platform compatibility
---------------------------------------

Orb is written in C++ and needs a C++11 compliant compiler to compile. There are no
other dependencies. 

Current platform build systems supported:
-Visual Studio 2013 (Win32)

Implementing cross compilation is a plausible exercise. If you are intrigued you
can just look up from the visual studio source files which solutions need which files and 
compile. 

There are three compile units currently present:
* 'orb' which consists of the language interpreter and compiles to a shared library
* 'orb-repl' which is simple Read-Eval-Print loop for the orb interpeter linking to the orb library.
* 'orb-test' which is a unit tester executable that when run will execute a set of unit tests.

Syntax
------
(Todo)
List ()
Vector []
Map {}

Embedding and extending
-----------------------
TODO

Using as a config parser
------------------------
TODO

References and Background
-------------------------
The main motivation in writing Orb was to test the use of a persisten map and list in
the context of C++ in writing an interpreter.

The secondary motivation was to have a structured configuration language,
that was easily embeddable to any C++ program.

The interpeter is based on the application
of a persistent map and list to create an execution context in 
native C++ that is isomporhic to that of Scheme. After this the interpreter itself
is almost a verbatim replication of the Scheme interpreter written
in Scheme in the "Structure and Interpretation of Computer Programs" [1].

The persistent map is an implementation of Bagwell's hash array mapped trie[3].
The implementation is heavily based in the Java version in Clojures sources.[4]

The idea of persistent maps is explained in [2]. Bagwell's implementation is nice because it uses really wide 
trees in a fairly efficient manner (thus the number of nodes duplicated at each new referenece is fairly small).

The interpreter follows typical conventions when writing compilers. Sestoft's introduction is fairly good in
this manner[5].

Included software
-----------------

Currently uses the Mersenne Twister random number library ( Copyright (C) 2011 Mutsuo Saito, Makoto Matsumoto) licenced under the BSD licence.

References
----------
[1] Gerald Sussman & Hal Abelson, "Structure and Interpretation of Computer Programs"

[2] James Driscoll, Neil Sarnak & Daniel Sleator, "Making Data Structures Persistent". Journal of Computer and System Sciences, Vol. 38, No. 1, Februrary 1989

[3] Phil Bagwell (2000), "Ideal Hash Trees" (Report). Infoscience Department, École Polytechnique Fédérale de Lausanne

[4] Rich Hickey, the Clojure programming language, https://github.com/clojure/clojure

[5] Peter Sestoft, "Programming Language Concepts", Springer 2012


