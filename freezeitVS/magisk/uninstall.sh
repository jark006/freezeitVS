#!/system/bin/sh

# This function is copied from [ Uperf@yc9559 ] module.
wait_until_login() {
    # in case of /data encryption is disabled
    while [ "$(getprop sys.boot_completed)" != "1" ]; do
        sleep 1
    done

    # we doesn't have the permission to rw "/sdcard" before the user unlocks the screen
    # shellcheck disable=SC2039
    local test_file="/sdcard/Android/.PERMISSION_TEST_FREEZEIT"
    true >"$test_file"
    while [ ! -f "$test_file" ]; do
        true >"$test_file"
        sleep 1
    done
    rm "$test_file"
}

remove_freezeit(){
    wait_until_login

    pidof freezeit | xargs kill -9
    pidof io.github.jark006.freezeit | xargs kill -9
    pm uninstall io.github.jark006.freezeit
    
    rm -rf /data/system/freezeit.conf
    rm -rf /data/system/freezeit
    rm -rf /sdcard/Android/freezeit*
}

(remove_freezeit &)
