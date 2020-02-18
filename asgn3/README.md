Samuel Shin
cruzid: sayshin
id num: 1510580

asgn3 contains: httpserver.cpp, Makefile, Design.pdf, Writeup.pdf

I would run my server like this: ./httpserver -c -l log.txt 127.0.0.1 8080.

These are the commands I used for GET and PUT
curl http://localhost:8080 --request-target 27longfilename
curl -v -T localfile http://localhost:8080 --request-target 27longfilename

and this might take a couple times to run also. When you see the 200/201 response code confirmed in the verbose, that's how I know it went through. When it freezes up just ctrl c and try again til u get the 
output or success response code

Old errors from before: 

When doing a GET request, it gets that weird binary error, but it looks like it's okay to have that error from Piazza. 
The server can only take in ip address in the xxx.x.x.x form and of the like and can't handle hostnames.
