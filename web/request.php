<?php
require_once __DIR__ . "/db.php";
$message = "";

if ($_SERVER["REQUEST_METHOD"] === "POST") {
    $bin = $_POST["bin"] ?? "";
    if (in_array($bin, ["A", "B", "C"], true)) {
        try {
            $conn = waste_db();
            $stmt = $conn->prepare("INSERT INTO request_history (bin_name) VALUES (?)");
            $stmt->bind_param("s", $bin);
            $stmt->execute();
            $stmt->close();
            $conn->close();

            $socket = @fsockopen("127.0.0.1", (int)(getenv("WASTE_TCP_PORT") ?: "5000"), $errno, $errstr, 2);
            $acknowledged = false;
            if ($socket) {
                stream_set_timeout($socket, 3);
                $line = "REQUEST@" . $bin . "\n";
                $acknowledged = fwrite($socket, $line) === strlen($line)
                    && fgets($socket, 8) === "OK\n";
                fclose($socket);
            }

            if ($acknowledged) {
                $message = $bin . " 쓰레기통 수거 요청이 관제 장치로 전송되었습니다.";
            } else {
                http_response_code(502);
                $message = "요청 기록은 저장됐지만 관제 장치로의 전송을 확인하지 못했습니다.";
            }
        } catch (RuntimeException $error) {
            error_log($error->getMessage());
            http_response_code(503);
            $message = "수거 요청 저장에 실패했습니다.";
        }
    } else {
        http_response_code(400);
        $message = "올바른 쓰레기통을 선택해 주세요.";
    }
}
?>
<!DOCTYPE html>
<html lang="ko">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>음식물 쓰레기 수거 요청</title>
<style>
* { box-sizing: border-box; }
body { margin:0; padding:20px; font-family:Arial,sans-serif; background:#f3f5f7; }
.container { max-width:500px; margin:0 auto; }
h1 { text-align:center; margin-bottom:10px; }
.description { text-align:center; color:#666; margin-bottom:30px; line-height:1.6; }
.bin { background:white; padding:20px; margin-bottom:18px; border-radius:15px; box-shadow:0 3px 10px rgba(0,0,0,.10); }
.bin h2 { margin-top:0; }
button { width:100%; padding:15px; border:none; border-radius:10px; font-size:18px; font-weight:bold; cursor:pointer; background:#ff6b6b; color:white; }
button:active { transform:scale(.98); }
.message { background:#dff5df; padding:15px; margin-bottom:20px; border-radius:10px; text-align:center; font-weight:bold; }
</style>
</head>
<body>
<div class="container">
<h1>음식물 쓰레기 수거 요청</h1>
<div class="description">음식물 쓰레기통이 가득 찼다면<br>해당 위치의 수거 요청 버튼을 눌러주세요.</div>
<?php if ($message !== "") { ?>
<div class="message"><?php echo htmlspecialchars($message); ?></div>
<?php } ?>
<?php foreach (["A","B","C"] as $b) { ?>
<div class="bin">
  <h2><?php echo $b; ?> 쓰레기통</h2>
  <form method="post">
    <button type="submit" name="bin" value="<?php echo $b; ?>"><?php echo $b; ?> 수거 요청</button>
  </form>
</div>
<?php } ?>
</div>
</body>
</html>
