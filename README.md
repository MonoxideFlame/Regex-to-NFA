# Objective:

Takes a Regex written in my own custom syntax and outputs an NFA using the thompson construction. 



\[\] <- contains a character set that will be matched. 
() <- groups a union. 

| <- a union of different strings. A union of different characters when inside \[\]. 

& <- Charset intersection

~ <- Set complement. 

\* <- kleene star
\+ <- Closure
? <- optional



Predefined Charsets:

```
#D: Digits
#U: Uppercase
#L: Lowecase
#W: Words [#D|#U|#L|_]
```







