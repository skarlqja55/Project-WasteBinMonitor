# 복구 검증

STM32는 10월 백업과 바이트가 동일한 LCD 원본을 `Core/Src/lcd1602.c`에 복구했습니다. I²C·GPIO·USART 설정, UART 수신 큐, TCP 수신 및 요청 응답, MariaDB 재현 스키마는 이번에 보완했습니다.

| 상태 | 범위 | 재실행 방법과 결과 |
|---|---|---|
| PASS | STM32 ARM GCC 14.2.1 | `make clean && make`로 ELF 생성 |
| PASS | 원본 LCD 및 UART HAL 모의 | `tests/mock_console.sh`로 LCD 부팅 문구·I²C 전송 주소, A/B/C 요청, Matrix·부저 호출, 5초 복귀, 센서 오류, 연속 4프레임 및 큐 초과 후 복구 확인 |
| PASS | Arduino Uno | `arduino-cli compile -b arduino:avr:uno arduino/waste_bin`의 Uno 빌드·링크 확인. AVR Core 1.8.7, WiFiEsp 2.2.2, MsTimer2 1.1 사용 |
| PASS | C TCP 서버 | `gcc -Wall -Wextra raspberry/food_server.c -o food_server $(mariadb_config --cflags --libs)`에 해당하는 컴파일·링크 검증. x86_64에서 MariaDB Connector/C 공유 라이브러리를 연결 |
| PASS | TCP 수신 코드 | `cc -ffunction-sections -fdata-sections tests/mock_tcp.c -Wl,--gc-sections,--wrap=read -o /tmp/waste-mock-tcp && /tmp/waste-mock-tcp`. 분할 메시지, LF/CRLF, 쓰기 종료, 수신 타임아웃, 버퍼 초과 확인 |
| PASS | TCP 루프백·RFCOMM 모의 | 호스트 Linux 루프백으로 서버에 SENSOR·REQUEST를 전송. DB 호출을 대체하고 RFCOMM 쓰기를 파일로 기록해 원래 STATUS·REQUEST 메시지 및 OK·ERR 응답 검증 |
| PASS | PHP 문법 | `php -l web/db.php`, `php -l web/index.php`, `php -l web/request.php` |
| PASS | Python 문법·헤드리스 데모 | `python3 -m py_compile route_view/route_view.py`, `python3 route_view/route_view.py --demo --camera -1 --no-window --out /tmp/waste-route`로 JPEG·JSON 출력 확인 |
| PASS | MariaDB 11.8 초기화 SQL | 격리 DB에서 `mariadbd --no-defaults --bootstrap --skip-networking`에 스키마 두 번 적용 및 세 테이블 삽입·기본 시각 쿼리 검증 |
| NOT_RUN | Apache·MariaDB·C 서버 종단 간 연결 | 실행 환경에서 MariaDB의 Unix 소켓 생성이 거부되어 bootstrap SQL 검증과 TCP 루프백 시험으로 나누어 실행 |
| NOT_RUN | CubeMX 재생성, NUCLEO·HC-06·LCD 및 센서 실물 | ST-LINK 플래시, I²C 풀업과 실제 100 kHz, UART 전기 신호와 Bluetooth 페어링, Matrix 점등, 부저 소리, Wi-Fi·배선·카메라 확인 필요 |

## UART 모의에서 확인한 범위

- 수신 데이터 한 줄은 최대 63바이트이며, 최대 7프레임을 대기시킵니다. 다음 프레임이 버퍼를 넘으면 카운터가 증가하고 프레임 끝에서 동기화합니다.
- 테스트는 Matrix·부저 함수 호출을 관찰합니다. 실제 광량·깜빡임·소리는 보드에서 확인해야 합니다.
- 원본 LCD의 `HAL_Delay()`와 전송 구조를 유지했습니다. 원본은 센서 오류 `-1`을 `-1%`로 표시합니다. I²C 오류 응답 처리와 LCD 블로킹 시간 단축은 별도 개선 후보입니다.
