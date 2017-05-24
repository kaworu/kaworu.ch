#!/usr/bin/env php
<?php
// PDO dsn
$pdo_dsn = 'pgsql:host=localhost;dbname=postfix;user=postfixadmin;password=secret';
// $rcmail_config['password_dovecotpw']
$dovecotpw = '/usr/local/bin/doveadm pw -r 12';
// $rcmail_config['password_dovecotpw_method']
$dovecotpw_method = 'BLF-CRYPT';
// where we log
$syslog_facility = LOG_MAIL;

// grab what we care about from the env.
$username  = getenv("USER");
$plainpass = getenv("PLAIN_PASS");

// init syslog.
$progname = basename(__FILE__);
openlog($progname, LOG_PID, $syslog_facility);

// connect to the database.
try {
    $dbh = new PDO($pdo_dsn);
    $dbh->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
} catch (PDOException $e) {
    syslog(LOG_CRIT, "database connection failed: {$e->getMessage()}");
    goto out;
}

try {
    // retrieve the user's current password.
    $sth = $dbh->prepare('SELECT password FROM mailbox WHERE username = ?');
    $sth->execute([$username]);
    $oldpasswd = $sth->fetchColumn();
    if (!$oldpasswd) {
        syslog(LOG_WARN, "unable to find the mailbox for $username");
        goto out;
    }

    // if we find the dovecot password method in the current password, it does
    // not need to be updated.
    if (strpos($oldpasswd, $dovecotpw_method) !== false) {
        syslog(LOG_INFO, "$username already use $dovecotpw_method, skipping.");
        goto out;
    }

    // generate a new password using `doveadm pw' without passing plainpass
    // on the cmdline for security concerns.
    $process = proc_open("$dovecotpw -s $dovecotpw_method", [
        0 => ['pipe', 'r'],
        1 => ['pipe', 'w'],
        2 => ['pipe', 'w'],
    ], $pipes);
    if (!is_resource($process)) {
        syslog(LOG_CRIT, "proc_open() failed.");
        goto out;
    }
    // $pipes now looks like this:
    // 0 => writeable handle connected to child stdin
    // 1 => readable handle connected to child stdout
    // 2 => readable handle connected to child stderr
    fclose($pipes[2]); // immediately close stderr as we don't need it.
    fwrite($pipes[0], "$plainpass\n");
    fwrite($pipes[0], "$plainpass\n");
    fclose($pipes[0]);
    $newpasswd = trim(stream_get_contents($pipes[1]));
    fclose($pipes[1]);
    $retval = proc_close($process);
    if ($retval !== 0) {
        syslog(LOG_ERR, "$dovecotpw exited with status $retval, expected 0.");
        goto out;
    }

    // sanity check to ensure that the new password has been computed with the
    // requested method.
    if (strpos($newpasswd, $dovecotpw_method) === false) {
        syslog(LOG_ERR, "unexpected $dovecotpw output.");
        goto out;
    }

    // update the password in the database with the newly computed one.
    $sth = $dbh->prepare('UPDATE mailbox SET password = :newpasswd WHERE username = :username AND password = :oldpasswd');
    $success = $sth->execute([
        ':newpasswd' => $newpasswd,
        ':username'  => $username,
        ':oldpasswd' => $oldpasswd,
    ]);

    // "close" the database connection,
    // see https://secure.php.net/manual/en/pdo.connections.php
    $sth = null;
    $dbh = null;
} catch (PDOException $e) {
    syslog(LOG_CRIT, "database query failed: {$e->getMessage()}");
    goto out;
}

if ($success) {
    syslog(LOG_INFO, "$username password successfully migrated to $dovecotpw_method.");
} else {
    syslog(LOG_CRIT, "$username password migration to $dovecotpw_method failed.");
}

// FALLTHROUGH
out: // cleanup.

// close syslog.
closelog();

/*
 * We have to continue execution from what we get on the command line argument,
 * i.e. $argv.
 *
 * see https://wiki.dovecot.org/PostLoginScripting
 */

// $argv[0] is our script (i.e. __FILE__), $argv[1] the next program to
// execute, and $argv[2..] the next program's arguments.
$next_exe  = $argv[1];
$next_argv = array_slice($argv, 2);
pcntl_exec($next_exe, $next_argv);
