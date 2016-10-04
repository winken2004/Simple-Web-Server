# Simple Web Server
---
### Overview

A simple web server on linux, test only on ubuntu14.04.

### Function

* GET a static object
	* get object with proper render.
	* 301 move permanently.
	* 403 forbidden.
	* 404 not found.
* GET a directory
	* search index.html
	* automaically generate index.html with hyperlinks if index.html not exist.
	* 403 forbidden.
	* 404 not found.

### Usage

* Compile

	> make

* Execute

	> ./webserver port-number path-to-web-root

* Clean

	> make clean

* Debug message 

	> make debug

* Simple demo

	> make  
	> mkdir test  
	> echo 'hello world!' > test/index.html  
	> ./webserver 5566 test

	Open web browser, connect `localhost:5566`.	
