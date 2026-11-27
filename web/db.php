<?php
function waste_db(): mysqli
{
    $password = getenv("WASTE_DB_PASSWORD");
    if ($password === false || $password === "") {
        throw new RuntimeException("Set WASTE_DB_PASSWORD");
    }

    $conn = new mysqli(
        getenv("WASTE_DB_HOST") ?: "127.0.0.1",
        getenv("WASTE_DB_USER") ?: "iot",
        $password,
        getenv("WASTE_DB_NAME") ?: "iotdb",
        (int)(getenv("WASTE_DB_PORT") ?: "3306")
    );
    $conn->set_charset("utf8mb4");
    return $conn;
}
