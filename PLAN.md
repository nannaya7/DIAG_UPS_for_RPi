# 수정계획 — UART 브리지 및 신규 핀맵 반영

> **진행 상태**: Step 1~4 완료, Keil 빌드 0 error. 구현은 DMA 대신 인터럽트+링버퍼로 변경(Step 3의 DMA 항목 대체). Step 5(접두사 라우팅)와 보드 실측 검증(2절)은 미완.

## 0. 현황 요약
- `.ioc`는 이미 신규 핀맵(USART1 PB6/7, USART2 PA2/3, USART3 PB10/11, PA11/PA15/PB13/PB14 등)이 반영되어 있음. 이번에 UART 3종의 115200/8N1 파라미터를 명시하고 function list(ADC1 제거, USART3 추가)를 정리함.
- 생성 코드(`main.h`, `main.c`, `hal_msp.c`, `it.c`)와 `SRC/`는 **구버전 핀맵·USART2 단일 구성** 그대로.
- 기존 USART2 = Radxa AT 프로토콜(`at_protocol.c`) 용도. 신규 구조에서 Radxa는 USART3, USART2는 ADuCM355로 역할이 바뀜.

## 1. 단계

### Step 1 — CubeMX 재생성 (`.ioc` 기준 동기화)
1. CubeMX에서 `.ioc`를 열어 아래를 GUI로 확인/설정 (수동 편집이 어려운 DMA/NVIC 항목):
   - USART2 RX: DMA1 Ch6 circular (기존 유지), USART2 global interrupt 활성.
   - USART3 RX: DMA1 Ch3 circular, USART3 global interrupt 활성.
   - (선택) USART1: 현재 미사용이므로 초기화만, 인터럽트/DMA 없음.
   - 클럭: 내장 HSI + PLL x16 = 64MHz (HSE 미실장).
   - ADC1: 신규 `.ioc`에는 없음. `DCDC_OUT_ADC`(회로도에 존재)를 계속 쓸지 결정 후 반영.
2. 코드 생성 → `main.h`의 핀 매크로가 신규 맵으로 갱신되는지 확인. `USER CODE` 블록(`MX_GPIO_Init_2`의 EXTI NVIC, `BATT_ALT` pull-up 등)이 보존/재적용되는지 점검.
3. `hal_msp.c`/`it.c`에 USART3, DMA1_Channel3 핸들러가 생성되는지 확인.

### Step 2 — 신규 핀맵을 SRC에 반영
- 매크로 이름은 유지되므로 대부분 자동 반영. 이름이 바뀐 것만 수정:
  - `USB_PWR_IN`(PA6) 신규 입력 — 용도 확인 후 사용처 결정 (현재 코드는 미사용).
  - `BOOST_EN`(PA15), `VOUT_CTRL`(PB14), `LDO_OUT`(PB13), `PWR_LED`(PA11) 위치 변경 확인 — `dcdc.c`, `power_control.c`.
  - `BATT_ALT`(PC15)가 PA15에서 이동 → EXTI15_10 → EXTI15_10 그대로이나 포트가 GPIOC. `power_control.c`의 `HAL_GPIO_EXTI_Callback`, `main.c`의 `HAL_GPIO_Init(GPIOA, BATT_ALT)`를 `BATT_ALT_GPIO_Port`로 교체. (PC15는 LSE 핀이므로 출력 전류 제한/슬루 제한 확인, 입력 용도는 문제 없음.)
  - PA15는 JTAG 핀 — SWD만 사용하므로 `__HAL_AFIO_REMAP_SWJ_NOJTAG()` 필요 (PA15 GPIO 사용 조건). CubeMX가 `SYS` 설정으로 처리하는지 확인.
  - `PWR_LED`(PA11) 제어 코드 필요 여부 확인 (기존 RGB LED와 별개).

### Step 3 — UART 브리지 구현 (현재 요구사항)
신규 모듈 `SRC/UART/uart_router.c/.h` (+ uvprojx에 등록):
1. USART3 / USART2 각각 RX = DMA circular + IDLE (`HAL_UARTEx_ReceiveToIdle_DMA`), 기존 `uart2_comm.c`의 wrap 처리 로직을 포트별 재사용 가능하게 일반화 (`UartPort_t` 구조: 핸들, rxBuf, readIdx).
2. `HAL_UARTEx_RxEventCallback`을 라우터로 일원화: 수신 포트에 따라
   - USART3 수신 -> USART2 송신
   - USART2 수신 -> USART3 송신
3. 송신: IDLE 콜백 안에서 블로킹 `HAL_UART_Transmit`은 바이트 유실 위험(두 포트 동일 속도라 보통 OK지만 안전하지 않음). **TX 링버퍼 + `HAL_UART_Transmit_IT`(또는 DMA)** 로 비동기 송신. 버퍼 크기 256B 이상, 오버플로 시 카운터 증가(디버그용).
4. 에러 처리: ORE/FE/NE 발생 시 `HAL_UART_ErrorCallback`에서 DMA RX 재시작.
5. 라우터에 `mode` 필드를 두어 `TRANSPARENT`(현재) / `PREFIX`(향후)로 전환 가능하게 구성.

### Step 4 — 기존 AT 프로토콜 처리 정리
- `Uart2Comm_*` / `AtProtocol_*`는 USART2 하드코딩. 브리지 동작 중 USART2 데이터는 모두 RPi로 투명 전달되어야 하므로 **`at_protocol` 수신 파싱을 USART2 경로에서 분리**.
- UPS 이벤트 송신(`AT+BTN0`, `AT+USB0`, `AT+BATxx` 등)을 RPi(USART3)로 계속 보낼지가 미결 — 브리지 데이터와 인터리브될 수 있음. 결정 전에는 `Uart2Comm_Send`를 USART3 송신 경로(TX 큐 경유)로 보내고, 브리지가 한 줄 단위로 끊기지 않는 점만 문서화.
- 접두사 없는 USART3 데이터가 UPS 자체 명령으로 확정되었으므로 AT 프로토콜은 USART3 기준으로 유지(수신 파싱 + 이벤트 송신 모두). 이벤트 송신은 TX 큐를 공유하므로 브리지 데이터와 줄 단위로 섞이지 않게 줄 경계에서 큐에 넣는다.
- 현재 투명 브리지 단계에서는 USART3 수신을 전부 USART2로 전달하고 AT 파싱·이벤트 송신은 비활성(확정). `at_protocol.c`는 삭제하지 말고 빌드에서만 제외/호출 중단해 두었다가 접두사 라우팅 도입 시 재활성화.

### Step 5 — 향후 확장 (접두사 라우팅, 이번 범위 아님)
- USART3 RX 스트림을 줄 단위(`\n`)로 조립 → `[AD]`/`[NF]` 접두사 판별 → 접두사 제거 후 해당 포트 TX 큐에 푸시.
- 접두사 없는 줄: UPS 보드 자체 명령 -> `AtProtocol_OnRxData`로 처리 (전달하지 않음). 확정됨.
- USART1(NFC) RX도 라우터에 등록해 USART3로 투명 전달. 포트 테이블 방식이므로 Step 3 구조 위에서 항목 추가만 필요.
- 접두사 판별 시 "줄 중간 끊김"(IDLE 시점이 줄 경계와 불일치) 대비 라인 버퍼 사용.

### Step 6 — 전원 관리 (향후, 이번 범위 아님)
- 방식: Cortex SLEEP(WFI)만 사용 (포트·RAM 유지, UART RX 웨이크). STOP/STANDBY 제외. 기존 `sleep_mode.c`(TIM3 1Hz 틱, `HAL_PWR_EnterSLEEPMode`)를 재활용하고 TIM3 분주값은 64MHz 기준(6399).
- 필요 작업: (1) `UartRouter_IsIdle()` 추가 — TX 링 비었고 USART TC 완료일 때만 슬립 진입, (2) 메인 루프에 슬립 진입 조건(LED 효과 종료, 전원 시퀀스 대기 없음) 추가, (3) 슬립 중 SysTick 1ms 웨이크 영향 검토, (4) 슬립 진입/복귀 시 GPIO 출력 불변과 UART 바이트 유실 없음 실측.
- 브리지 구조(인터럽트+링버퍼)는 이 요구에 맞게 설계되어 있어 변경 불필요 (DMA 미사용이라 DMA 클럭 의존도 없음).

## 2. 검증
- USART3 <-> USART2에 USB-UART 어댑터 2개를 물려 루프 확인: 양방향, 1바이트/256B 기본 패킷(및 256B 연속 버스트, 링버퍼 wrap 경계)/연속 전송.
- 115200 풀 스피드 연속 송신 시 바이트 유실 없는지 (카운터/체크섬).
- 전원 시퀀스(버튼, USB 플러그, 배터리 알림)가 브리지 추가 후에도 동작하는지.
- Keil 빌드 경고 0, 플래시/RAM 사용량 확인.

## 3. 확정 사항 / 미결 사항
확정:
1. 클럭: HSE 미실장 -> 내장 HSI + PLL x16 = 64MHz (`.ioc` 반영 완료). `sleep_mode.c` TIM3 분주값은 64MHz 기준(6399).
2. 현재 단계에서 UPS 자체 명령은 사용하지 않음 -> 브리지 중 USART3 데이터는 전부 USART2로 전달, AT 파싱 비활성(필요 시 추가). AT 이벤트 송신(버튼/USB/배터리)도 현재는 제거/비활성 대상(브리지 데이터 오염 방지).
3. ADC 미사용 -> `power_adc.c/.h`, `hadc1`, `MX_ADC1_Init`, ADC MSP, `stm32f1xx_hal_adc*.c`를 빌드에서 제거. `dcdc.c`/`power_control.c`의 ADC 의존부 정리.
4. MCU: STM32F103CB (flash 128KB, RAM 20KB). `.ioc`는 CB로 갱신함. Keil 프로젝트(`uvprojx`)의 Device(현재 F103C8, IROM 64KB)는 Keil에서 STM32F103CB로 변경 필요.

미결(향후):
- 접두사 없는 USART3 데이터는 향후 UPS 자체 명령(AT 프로토콜)으로 처리 (확정, 구현은 필요 시).
