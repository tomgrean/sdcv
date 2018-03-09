/*
    writer : Opera Wang
    E-Mail : wangvisual AT sohu DOT com
    License: GPL
*/

/* filename: distance.cc */
/*
http://www.merriampark.com/ld.htm
What is Levenshtein Distance?

Levenshtein distance (LD) is a measure of the similarity between two strings, 
which we will refer to as the source string (s) and the target string (t). 
The distance is the number of deletions, insertions, or substitutions required
 to transform s into t. For example,

    * If s is "test" and t is "test", then LD(s,t) = 0, because no transformations are needed. 
    The strings are already identical.
    * If s is "test" and t is "tent", then LD(s,t) = 1, because one substitution
     (change "s" to "n") is sufficient to transform s into t.

The greater the Levenshtein distance, the more different the strings are.

Levenshtein distance is named after the Russian scientist Vladimir Levenshtein,
 who devised the algorithm in 1965. If you can't spell or pronounce Levenshtein,
 the metric is also sometimes called edit distance.

The Levenshtein distance algorithm has been used in:

    * Spell checking
    * Speech recognition
    * DNA analysis
    * Plagiarism detection 
*/

