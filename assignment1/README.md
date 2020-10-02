# Usage

Run a mock HDFS setup, complete with Namenode, Datanodes and HDFS client app. Run as separate Python processes on a single machine (all communication done with respect to localhost and ports).

Spin up Namenode first before Datanodes. Requires Python 3 (tested on Python 3.7).

To spin up Namenode:

```
python namenode.py <dir_path>
```

`<dir_path>` is where the `fsimage binary` file will be saved for persistent use between sessions. Note that the Namenode is hardcoded to use port 60420 on localhost, so preferably don't have any other service using that port.

To spin up Datanode:

```
python datanode.py <dir_path> <port>
```

`<dir_path>` is where the `hdfs_config.json` file will be saved, along with a `blocks` dir where binary blocks will be saved on disk. Please ensure multiple Datanodes don't share the same path. Ideally spin up 2-3 Datanodes at a time. `port` must be an unused port on the localhost. Specify same `<dir_path>` across sessions to retain persistent settings and block storage.

To spin up an interactive client:

```
python client.py
```

Use `help` in the client to see available commands. Commandline in the client is very finnicky, so don't get too fancy with it.
