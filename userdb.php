<?php


////////////////////////////////////////////////////////////////

/*
    DSN:

$dsn = array(
    // internal DSN name
    'id' => 'some_id_or_name',

    // DB driver wrapper name
    'driver' => 'mysql',

    // DSN itself
    'host' => 'ip',
    'port' => 'port',
    'username' => 'user',
    'password' => 'pwd',
    'dbname' => 'database_name',
);


    DB driver wrapper class name:

class_name = 'dd_' + driver_name

*/

function createDBDriver($dsn) {
    $ddclassname = 'dd_' . $dsn['driver'];
    return new $ddclassname($dsn);
}

////////////////////////////////////////////////////////////////
// MySQL database driver

class dd_mysql {
    // Create DB driver with given DSN
    function __construct($dsn) {
        $this->dsnid = $dsn['id'];
        mysqli_report(MYSQLI_REPORT_STRICT);    // set mysqli to use exceptions instead of warnings
        try {
            $this->ms = new mysqli($dsn['host'], $dsn['username'], $dsn['password'], $dsn['dbname'], $dsn['port']);
        } catch (mysqli_sql_exception $e) {
            throw new Exception("Couldn't connect to ds-".$dsn['id']." at ".$dsn['host'].":".$dsn['port']."/".$dsn['dbname'].".");
        }
    }

    function __destruct() {
    }

    // Query methods

    function queryOpen($query_string) {
        $query = $this->ms->query($query_string);
        return ($query === false) ? null : $query;      // replace false (if any) with null
    }

    function queryClose(&$query) {
        $query->close();
        $query = null;
    }

    function fetchNextRecord($query) {
        return $query->fetch_assoc();
    }

    // Simple Query - retrieve one record

    function simpleQuery($query_string) {
        $query = $this->ms->query($query_string);
        if($query !== false) {
            $result = $query->fetch_assoc();
            $query->close();
            return $result;
        }
        return null;
    }
}



////////////////////////////////////////////////////////////////
// Main database

class MasterDB {
    function __construct($dsn) {
        $this->dd = createDBDriver($dsn);
        $this->dsns = array();
    }

    function __destruct() {
    }

    // retrieve total user count
    function getUserCount() {
        $query_string = "SELECT count(`uid`) as `count` FROM `uids`";
        $result = $this->dd->simpleQuery($query_string);
        if(!$result) {
            throw new Exception("Couldn't get user count.");
        }
        return (int) $result['count'];
    }

    // get all user ids as an array
    function getUserIDList() {
        $query_string = "SELECT `uid` FROM `uids`";

        $query = $this->dd->queryOpen($query_string);
        if (!$query) {
            throw new Exception("Couldn't get userid list.");
        }
        $list = array();
        while ($row = $this->dd->fetchNextRecord($query)) {
            $list[] = $row['uid'];
        }
        $this->dd->queryClose($query);

        return $list;
    }

    // get/create DB driver for data source where user data are stored
    function getDBForUser($userid) {
        $uid = (int) $userid;
        $query_string = "SELECT `dsnid` FROM `uids` WHERE `uid`=$uid";

        $result = $this->dd->simpleQuery($query_string);
        if(!$result) {
            throw new Exception("Couldn't get DSN id for user#$uid.");
        }

        $dsnid = $result['dsnid'];
        if(isset($this->dsns[$dsnid]) == false) {
            $query_string = "SELECT * FROM `dsns` WHERE `id`=$dsnid";

            $result = $this->dd->simpleQuery($query_string);
            if(!$result) {
                throw new Exception("Couldn't get DSN for user#$uid.");
            }
            $this->dsns[$dsnid] = createDBDriver($result);
        }
        return $this->dsns[$dsnid];
    }
}

function createDBMaster() {
    $dsn_master = array(
        'id' => 'master',
        'driver' => 'mysql',
        'host' => 'localhost',
        'port' => '3306',
        'username' => 'root',
        'password' => 'passwd',
        'dbname' => 'testusers',
    );
    return new MasterDB($dsn_master);
}




////////////////////////////////////////////////////////////////
// entry

function printUser($dd, $userid) {
    $uid = (int) $userid;
    $query_string = "SELECT * FROM `users` WHERE `userid`=$uid";
    $result = $dd->simpleQuery($query_string);
    echo "User #$uid: " . $result['name'] . " " . $result['lastname'] . " (" . $result['dob'] . ")" . PHP_EOL;
}


try {
    $mdb = createDBMaster();
    $numUsers = $mdb->getUserCount();
    echo "Total users: $numUsers" . PHP_EOL;
    $useridList = $mdb->getUserIDList();
    //var_dump($useridList);
    foreach ($useridList as $userid) {
        try {
            $dd = $mdb->getDBForUser($userid);
            if($dd) {
                printUser($dd, $userid);
            }
        }
        catch(Exception $e) {
            echo '<ERROR>: ', $e->getMessage(), PHP_EOL;
        }
    }
}
catch(Exception $e) {
    echo '<ERROR>: ', $e->getMessage(), PHP_EOL;
}





echo "----------------------------------------------------------------" . PHP_EOL;
?>
