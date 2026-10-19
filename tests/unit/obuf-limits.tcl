start_server {tags {"obuf-limits"}} {
    test {Client output buffer hard limit is enforced} {
        r config set client-output-buffer-limit {pubsub 100000 0 0}
        set rd1 [redis_deferring_client]

        $rd1 subscribe foo
        set reply [$rd1 read]
        assert {$reply eq "subscribe foo 1"}

        # On some systems (macOS for instance) the loopback socket buffers can
        # absorb tens of megabytes, so publishing tiny messages would need
        # millions of iterations before omem starts to grow and the test never
        # converges. Use a big payload instead: a few dozens of publishes are
        # enough to cross the 100k hard limit.
        set payload [string repeat "x" 10000]
        set omem 0
        for {set i 0} {$i < 1000} {incr i} {
            r publish foo $payload
            set clients [split [r client list] "\r\n"]
            set c [split [lindex $clients 1] " "]
            if {![regexp {omem=([0-9]+)} $c - omem]} break
            if {$omem > 200000} break
        }
        assert {$omem >= 70000 && $omem < 200000}
        $rd1 close
    }

    test {Client output buffer soft limit is not enforced if time is not overreached} {
        r config set client-output-buffer-limit {pubsub 0 100000 10}
        set rd1 [redis_deferring_client]

        $rd1 subscribe foo
        set reply [$rd1 read]
        assert {$reply eq "subscribe foo 1"}

        # Push omem above the soft limit with a big payload first, otherwise
        # tiny messages may never fill the socket buffers on some systems.
        set payload [string repeat "x" 10000]
        set omem 0
        for {set i 0} {$i < 1000 && $omem <= 100000} {incr i} {
            r publish foo $payload
            set clients [split [r client list] "\r\n"]
            set c [split [lindex $clients 1] " "]
            if {![regexp {omem=([0-9]+)} $c - omem]} break
        }
        # The soft limit is only evaluated while new replies are produced, so
        # keep publishing (small messages) while measuring the elapsed time.
        set start_time [clock seconds]
        set time_elapsed 0
        while 1 {
            r publish foo bar
            set clients [split [r client list] "\r\n"]
            set c [split [lindex $clients 1] " "]
            if {![regexp {omem=([0-9]+)} $c - omem]} break
            set time_elapsed [expr {[clock seconds]-$start_time}]
            if {$time_elapsed >= 5} break
        }
        assert {$omem >= 100000 && $time_elapsed >= 5 && $time_elapsed <= 10}
        $rd1 close
    }

    test {Client output buffer soft limit is enforced if time is overreached} {
        r config set client-output-buffer-limit {pubsub 0 100000 3}
        set rd1 [redis_deferring_client]

        $rd1 subscribe foo
        set reply [$rd1 read]
        assert {$reply eq "subscribe foo 1"}

        # Same as above: cross the soft limit quickly with a big payload.
        set payload [string repeat "x" 10000]
        set omem 0
        for {set i 0} {$i < 1000 && $omem <= 100000} {incr i} {
            r publish foo $payload
            set clients [split [r client list] "\r\n"]
            set c [split [lindex $clients 1] " "]
            if {![regexp {omem=([0-9]+)} $c - omem]} break
        }
        # Keep publishing until the soft limit time (3 seconds) is overreached
        # and the server closes the client.
        set start_time [clock seconds]
        set time_elapsed 0
        while 1 {
            r publish foo bar
            set clients [split [r client list] "\r\n"]
            set c [split [lindex $clients 1] " "]
            if {![regexp {omem=([0-9]+)} $c - omem]} break
            set time_elapsed [expr {[clock seconds]-$start_time}]
            if {$time_elapsed >= 10} break
        }
        assert {$omem >= 100000 && $time_elapsed < 6}
        $rd1 close
    }

    test {No response for single command if client output buffer hard limit is enforced} {
        r config set client-output-buffer-limit {normal 100000 0 0}
        # Total size of all items must be more than 100k
        set item [string repeat "x" 1000]
        for {set i 0} {$i < 150} {incr i} {
            r lpush mylist $item
        }
        set orig_mem [s used_memory]
        # Set client name and get all items
        set rd [redis_deferring_client]
        $rd client setname mybiglist
        assert {[$rd read] eq "OK"}
        $rd lrange mylist 0 -1
        $rd flush
        after 100

        # Before we read reply, redis will close this client.
        set clients [r client list]
        assert_no_match "*name=mybiglist*" $clients
        set cur_mem [s used_memory]
        # 10k just is a deviation threshold
        assert {$cur_mem < 10000 + $orig_mem}

        # Read nothing
        set fd [$rd channel]
        assert_equal {} [read $fd]
    }

    # Note: This test assumes that what's written with one write, will be read by redis in one read.
    # this assumption is wrong, but seem to work empirically (for now)
    test {No response for multi commands in pipeline if client output buffer limit is enforced} {
        r config set client-output-buffer-limit {normal 100000 0 0}
        set value [string repeat "x" 10000]
        r set bigkey $value
        set rd1 [redis_deferring_client]
        set rd2 [redis_deferring_client]
        $rd2 client setname multicommands
        assert_equal "OK" [$rd2 read]

        # Let redis sleep 1s firstly
        $rd1 debug sleep 1
        $rd1 flush
        after 100

        # Create a pipeline of commands that will be processed in one socket read.
        # It is important to use one write, in TLS mode independant writes seem
        # to wait for response from the server.
        # Total size should be less than OS socket buffer, redis can
        # execute all commands in this pipeline when it wakes up.
        set buf ""
        for {set i 0} {$i < 15} {incr i} {
            append buf "set $i $i\r\n"
            append buf "get $i\r\n"
            append buf "del $i\r\n"
            # One bigkey is 10k, total response size must be more than 100k
            append buf "get bigkey\r\n"
        }
        $rd2 write $buf
        $rd2 flush
        after 100

        # Reds must wake up if it can send reply
        assert_equal "PONG" [r ping]
        set clients [r client list]
        assert_no_match "*name=multicommands*" $clients
        set fd [$rd2 channel]
        assert_equal {} [read $fd]
    }

    test {Execute transactions completely even if client output buffer limit is enforced} {
        r config set client-output-buffer-limit {normal 100000 0 0}
        # Total size of all items must be more than 100k
        set item [string repeat "x" 1000]
        for {set i 0} {$i < 150} {incr i} {
            r lpush mylist2 $item
        }

        # Output buffer limit is enforced during executing transaction
        r client setname transactionclient
        r set k1 v1
        r multi
        r set k2 v2
        r get k2
        r lrange mylist2 0 -1
        r set k3 v3
        r del k1
        catch {[r exec]} e
        assert_match "*I/O error*" $e
        reconnect
        set clients [r client list]
        assert_no_match "*name=transactionclient*" $clients

        # Transactions should be executed completely
        assert_equal {} [r get k1]
        assert_equal "v2" [r get k2]
        assert_equal "v3" [r get k3]
    }
}
