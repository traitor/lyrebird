# Lyrebird

Introduction
------------
Lyrebird is a program suite for performing decryption of specially encrypted tweets. It utilizes the client-server model to spread the workload across multiple computers. It will efficiently use all available cores to speed up the process.

This program has been developed using requirements specified by Brian Booth.

Description
-----------
Lyrebird is split into two programs - `lyrebird.server` and `lyrebird.client`. To use this software, one must run the server and client separately.

#### Server
The server will read from a configuration file which has a list of files to decrypt. It will assign these to any connected clients using the First Come First Serve scheduling algorithm.

#### Client
The clients will receive these files and perform the actual decryption on the local machine. Each client creates a set of children, optimally using all cores and processors of the machine. The children will communicate with the parent (client) to receive files and perform the decryption.

Multiple clients can connect to one server to maximize the overall speed of decryption. Once all files have been decrypted, the clients will exit, then the server will terminate after ensuring all clients have closed successfully.

Instructions
------------
**Note: You have to be on a Linux computer**

To compile the source, run the `make` command. This will generate both `lyrebird.server` and `lyrebird.client`.


Before starting the server, you must generate a configuration file, which will specify the locations of the encrypted files and the locations of the decrypted contents.

Here is a list of possible files:

* `encrypted.txt output.txt`
* `./tweet.txt tweet_decrypted.txt`
* `~/input.txt ~/message.txt`

You must ensure that the same files are located in the correct locations on the computer(s) running the lyrebird client. Once ready, start the lyrebird server with by the following:

```
./lyrebird.server [Configuration File] [Log File]
```

The server will log important events to the log file, such as server information clients connecting or disconnecting, status of file decryption and more. Once the server is running, it will automatically grab the IP address of the active network adapter. This will be output as well as the randomly assigned port address.

To launch the lyrebird client, you must specify the IP address and port number of the server:

```
./lyrebird.client [IP address] [Port Number]
```

Once the client successfully connects, the server will begin messaging the client files to decrypt. Once the server has sent out all files, it will signal the client to terminate. If the client has not encountered an error it will exit and then the server will terminate.


Sources
-------
For modular exponentiation/exponentiation by squaring: [Link](http://homepages.math.uic.edu/~leon/cs-mcs401-s08/handouts/fastexp.pdf)