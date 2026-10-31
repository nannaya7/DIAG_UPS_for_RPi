# CLAUDE.md

Raspberry Pi(Radxa) 전원 공급용 UPS 보드(DIAG_UPS v1.2)의 STM32F103CB 펌웨어.
툴체인: Keil MDK-ARM V5 (`MDK-ARM/cappic_UPS_stm32f103.uvprojx`), STM32Cube HAL (FW_F1 V1.8.7), CubeMX 6.18 (`DIAG_UPS_stm32f103.ioc`).
회로도: `DIAG_UPS1.2_SCH.pdf`. 문서: `docs/` (AT 프로토콜, 펌웨어 기술 레퍼런스, Radxa 매뉴얼).

## 디렉터리
- `Core/` — CubeMX 생성 코드 (main.c, stm32f1xx_it.c, stm32f1xx_hal_msp.c). `USER CODE` 블록 밖은 재생성 시 덮어써짐.
- `SRC/` — 애플리케이션 코드. 새 폴더 추가 시 uvprojx의 IncludePath/파일 그룹에도 등록해야 함.
  - `myapp.c` 메인 스케줄러(`App_Start`), `POWER/` 전원 시퀀스·DCDC·ADC·슬립, `BATT/` MAX17048, `LED/` RGB LED(TIM1 PWM), `UART/` 통신, `DEBUG/` SEGGER RTT 로그.
- `MDK-ARM/` — Keil 프로젝트와 빌드 산출물 (산출물은 수정/커밋 대상 아님).

## UART 구성 (모두 115200bps, 8N1)
| UART | 핀 | 연결 대상 | 상태 |
|---|---|---|---|
| USART1 | PB6(TX)/PB7(RX) — AFIO 리맵 | NFC | 현재 미사용 (초기화만) |
| USART2 | PA2(TX)/PA3(RX) | ADuCM355 | 사용 |
| USART3 | PB10(TX)/PB11(RX) | Raspberry Pi | 사용 |

### 현재 동작: USART3 <-> USART2 투명 브리지
- 가공 없이 바이트 그대로 양방향 전달 (USART3 RX -> USART2 TX, USART2 RX -> USART3 TX).
- 구현: `SRC/UART/uart_router.c` — DMA 없이 RX 인터럽트로 받은 바이트를 상대 포트 TX 링버퍼(512B)에 넣고 TXE 인터럽트로 송신. `USART2_IRQHandler`/`USART3_IRQHandler`(it.c)가 `UartRouter_IRQHandler`를 호출. 초기화는 `App_Start()` 안의 `UartRouter_Init()`.
- 기본 데이터 단위는 256바이트. 브리지는 바이트 단위 전달이라 길이 제한은 없고, TX 링(512B)은 256B 패킷 2개분을 버퍼링한다. 접두사 라우팅 도입 시 줄/패킷 버퍼는 256B 이상으로 잡는다.
- 링버퍼 생산/소비는 모두 같은 우선순위(5) ISR에서만 일어나 락이 없다. ISR 밖(메인 루프)에서 push하려면 인터럽트 금지 구간이 필요하다.

### 향후 동작: 접두사 라우팅 (USART3 기준)
- USART3 수신 `[AD]...` -> 접두사 제거 후 USART2(ADuCM355)로 전달.
- USART3 수신 `[NF]...` -> 접두사 제거 후 USART1(NFC)로 전달.
- USART2 / USART1 수신 데이터 -> 가공 없이 USART3로 전달.
- 접두사 없는 USART3 데이터는 **UPS 보드 자체 명령**이다 -> 기존 `at_protocol`(AT+...) 파서로 처리하고 어느 UART로도 전달하지 않는다.
- 단, 현재 단계(투명 브리지)에서는 UPS 자체 명령을 사용하지 않는다: USART3 데이터는 전부 USART2로 그대로 전달하고 AT 파싱/이벤트 송신은 비활성. 필요해지면 추가한다 (`at_protocol.c`는 삭제하지 말고 보존).
- 라우팅 로직은 `SRC/UART/`의 별도 모듈(예: `uart_router.c`)에 두고, 브리지 단계에서는 동일 모듈의 "투명 모드"로 구현해 이후 확장이 쉽도록 한다.

## GPIO 핀맵 (.ioc 기준, 구버전에서 변경됨)
| 신호 | 핀 | 방향/비고 |
|---|---|---|
| BTN_PWR_IN | PA0 | EXTI0 (rising) |
| USB_PWR_IN | PA6 | 입력 |
| BATT_ADC_CTRL | PA7 | 출력 |
| PWM_CH1/2/3 | PA8/PA9/PA10 | TIM1 CH1~3 (RGB LED) |
| PWR_LED | PA11 | 출력 |
| BOOST_EN | PA15 | 출력 |
| USB_PLUG_IN | PB0 | 입력 |
| CHG_WKUP | PB12 | open-drain 출력, 평소 SET(해제) |
| LDO_OUT | PB13 | 출력 |
| VOUT_CTRL | PB14 | 출력 |
| I2C1 SCL/SDA | PB8/PB9 | AFIO 리맵, MAX17048 |
| BATT_ALT | PC15 | EXTI15 (falling, pull-up 필요 — MAX17048 ALRT open-drain) |
| SWD | PA13/PA14 | 디버그 |

핀 매크로(`*_Pin`, `*_GPIO_Port`)는 `Core/Inc/main.h`에 정의되며 `.ioc` 라벨에서 생성된다. `main.h`와 `.ioc`가 어긋나지 않게 유지할 것.

## 작업 규칙
- 핀/UART/클럭 변경은 **`.ioc`를 먼저 수정**하고, 코드(`main.c`, `main.h`, `hal_msp.c`, `it.c`)를 .ioc와 일치시킨다. 수동 수정은 반드시 `USER CODE` 블록 안에 넣는다.
- 클럭은 **내장 HSI(8MHz/2) + PLL x16 = 64MHz** (HSE 수정진동자는 미실장이라 사용하지 않음, `.ioc`의 HSE 끔). APB1 32MHz, APB1 타이머 64MHz. F103의 HSI 경로 최대치가 64MHz라 72MHz는 불가. HSE 전제로 쓴 코드(72MHz 가정)를 새로 넣지 말 것. HSI는 정밀도가 ±1% 수준이라 UART 115200 오차에 유의(필요하면 `HSICalibration` 확인).
- MCU는 STM32F103CB (flash 128KB / RAM 20KB). Keil Device 설정이 아직 F103C8(64KB)이면 CB로 맞출 것.
- ADC는 사용하지 않는다 (DCDC_ADC 미사용). ADC 관련 코드는 제거 대상.
- UART RX 콜백/ISR에서는 블로킹 호출(긴 `HAL_UART_Transmit`, RTT 출력 등)을 피한다. 브리지 지연·바이트 유실의 원인이 된다.
- 인터럽트 우선순위: EXTI 3, USART/DMA 5, TIM3 6, SysTick 15 (현재 값 — 변경 시 서로 영향 확인).
- 빌드/다운로드는 Keil에서 수행 (CLI 빌드 환경 없음). 코드 수정 후 빌드 검증은 사용자에게 요청하거나 UV4 CLI 사용 가능 여부 확인.
- 디버그 로그는 SEGGER RTT (`DebugLog_Printf`). UART는 로그에 쓰지 않는다 (브리지 데이터와 섞임).

## 향후: 전원 관리 (슬립)
- 미사용 시 **포트(GPIO 출력 상태)와 RAM을 유지한 채 슬립**한다 -> Cortex-M SLEEP(WFI, `SleepMode_Enter()`)를 사용. STOP/STANDBY는 사용하지 않는다 (STANDBY는 RAM/GPIO 소실, STOP은 클럭 정지로 UART 브리지·PLL 재설정 필요).
- 현재는 `myapp.c` 메인 루프에서 `SleepMode_Enter()`가 비활성(LED 효과가 `HAL_GetTick` 기반이라). 도입 시점의 조건: LED 효과/전원 시퀀스가 없을 때만 진입.
- 슬립 중에도 UART 브리지가 동작해야 한다: SLEEP 모드는 USART/DMA/TIM 클럭이 유지되므로 RX 인터럽트가 MCU를 깨운다. 따라서
  - 슬립 진입 전 TX 링버퍼가 비었는지 확인 (`UartRouter`에 `IsIdle()` 추가 예정).
  - USART 클럭/핀을 슬립 진입 시 끄지 않는다. 주변장치 클럭 게이팅은 USART2/3 외 항목(I2C, TIM1 등)에만 적용.
  - 깨우는 소스: USART2/3 RX, EXTI0(버튼), EXTI15(BATT_ALT), TIM3 1Hz 틱. SysTick(1ms)도 깨우므로 필요 시 슬립 중 SysTick 정지 검토.
- 이 항목은 아직 구현 전이다. 새 기능 추가 시 위 제약(포트/RAM 유지, UART 웨이크)을 깨지 않게 한다.

## Git 커밋 규칙
- 커밋을 만들 때마다 **먼저 사용자에게 버전을 물어본다** (이전 버전을 임의로 재사용하거나 추측해서 올리지 않는다).
- 커밋 메시지 제목 앞에 버전 접두사를 `[V.0.xx.xxx]` 형식으로 붙인다. 예: `[V.0.01.003] UART3-UART2 브리지 구현`
- 접두사 뒤에 한 칸 띄우고 변경 내용을 쓴다. 사용자가 요청할 때만 커밋한다.

## 현재 알려진 불일치 / 주의
- `main.h`, `hal_msp.c`, 일부 SRC 코드는 아직 **구버전 핀맵** (BOOST_EN=PA6, VOUT_CTRL=PB1, LDO_OUT=PB11, ADC 핀 PA4/PA5, 단일 USART2 라인 등)을 사용 중. 수정계획은 `PLAN.md` 참고.
- 기존 `at_protocol.c`는 USART2에서 Radxa와 AT 명령을 주고받던 구조. USART3(RPi) 이전 후 AT 프로토콜은 USART3의 "접두사 없는 자체 명령" 경로가 된다. 브리지 단계에서의 공존 방식은 `PLAN.md` 참고.
