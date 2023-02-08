start_server {tags {"dummy"}} {
    test {SET and GET an item} {
        r set x foobar
        r get x
    } {foobar}
}