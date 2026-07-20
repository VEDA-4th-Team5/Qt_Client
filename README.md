# Qt Client

Qt Widgets와 QML 기반 스마트 주차 관제 클라이언트입니다.

## 기능

- 한화비전 카메라 RTSP 4채널 표시
- 채널 확대·복귀 시 기존 스트림 유지
- EV-01~EV-03, P-01~P-04 주차면 상태 표시
- Raspberry Pi HTTP API에서 주차 상태 동기화
- 차량번호, 차량 유형, 점유 시간과 경고 표시
- 주차면 클릭 시 차량·번호판 원본/개선 이미지 조회
- API 실패 시 Mock 화면 유지
- 이벤트 로그와 CSV 내보내기

## 팀 공유 API 설정

config/client_config.ini은 Git으로 추적되는 팀 공용 설정입니다.

~~~ini
[api]
enabled=true
base_url=http://172.20.35.167:8080
slots_path=/api/v1/parking-slots
slot_detail_path=/api/v1/parking-slots/{slot_id}
timeout_ms=5000
reconnect_interval_ms=5000
max_reconnect_interval_ms=60000
allow_insecure_http=true
~~~

개인 PC에서만 주소를 변경하려면 다음 파일을 복사합니다.

~~~powershell
Copy-Item ..\config\client_config.local.example.ini ..\config\client_config.local.ini
~~~

client_config.local.ini은 Git에서 제외되며, 공용 파일보다 우선합니다.

Settings 화면의 `Server API URL`에서 주소를 저장하면 로컬 오버라이드 파일에 기록되고 즉시 재연결합니다. 연결 실패 시 5초부터 최대 60초까지 지수 백오프로 자동 재시도하며 `Reconnect now` 버튼으로 즉시 다시 연결할 수 있습니다.

## 소스 구조

- `src/mainwindow.*`: 사이드바, 페이지 전환, 화면 간 signal 연결
- `src/pages/`: Dashboard, Parking Map, Events, Settings, Debug 화면
- `src/controllers/parkingcontroller.*`: API·Mock 데이터와 주차 상태/알람 처리
- `src/dialogs/slotevidencedialog.*`: 차량·번호판 이미지 증거 화면
- `src/models/parkingstate.*`: 화면에서 공유하는 주차 상태 모델
- `src/services/camerasettings.*`: 카메라 설정 파일과 RTSP URL 구성
- `src/api/`: HTTP 요청, 이미지 로딩, JSON 응답 파싱

각 페이지는 사용자 동작을 signal로 전달하고, 상태 변경은 `ParkingController`를 거쳐 화면에 반영됩니다.
## 빌드

~~~bash
cmake -S . -B build
cmake --build build
~~~

Qt 또는 FFmpeg 경로를 직접 지정해야 하면:

~~~bash
cmake -S . -B build   -DCMAKE_PREFIX_PATH=/path/to/Qt   -DFFMPEG_ROOT=/path/to/ffmpeg
cmake --build build
~~~

## 실행

~~~bash
./build/smart_parking_qt_client
~~~

API 규격은 [docs/api_v1.md](docs/api_v1.md), RTSP 동작은
[docs/rtsp_notes.md](docs/rtsp_notes.md)를 참고합니다.
