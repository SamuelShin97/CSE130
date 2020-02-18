Samuel Shin
cruzid: sayshin
id num: 1510580

asgn2 contains: httpserver.cpp, Makefile, Design.pdf, Writeup.pdf

Threading broke all of my error code stuff, so I don't think any of it works. When testing, sometimes it takes multiple times for the commands to work. I had a shell script that had: 
curl -v -T http://localhost:8080 --request-target filename &
curl -v -T http://localhost:8080 --request-target filename &

This works it just takes a couple of times to work.

I would run my server like this: ./httpserver -N8 -l log.txt 127.0.0.1 8080. If -N by itself works and gives 4 threads by default. But if you want to specify a thread amount, make sure there isn't a space
between the N and number as above. 

To test PUT functions, the shell scripts don't work for me because PUT commands never terminated for me so I have to ctrl c to get out of them. So I would have multiple terminals up and run these commands
one after another really fast. I would run them as fast as I could click into the windows. 
E.G.
curl -v -T localfile http://localhost:8080 --request-target 27longfilename (in one terminal)
curl -v -T localfile2 http://localhost:8080 --request-target 27longfilename2 (in another terminal)

and this might take a couple times to run also. When you see the 201 created confirmed in the verbose, that's how I know it went through. But I've seen this work on multiple threads, and when a request has 
to wait for a thread to come open. I hope you guys see this working too. 

Old errors from before: 

When doing a GET request, it gets that weird binary error, but it looks like it's okay to have that error from Piazza. 
The server can only take in ip address in the xxx.x.x.x form and of the like and can't handle hostnames.
