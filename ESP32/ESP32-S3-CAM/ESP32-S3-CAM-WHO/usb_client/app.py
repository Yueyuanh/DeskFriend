#!/usr/bin/env python3
"""DeskFriend 有线客户端：UVC 画面 + CDC 遥测/控制。

一块 ESP32-S3 板子通过 OTG 口同时暴露两个 USB 功能：

* UVC 摄像头（MJPG）—— 用 OpenCV 打开，显示实时画面；
* CDC ACM 串口 —— 每 500 ms 收到一行 JSON（芯片温度、分辨率、帧率、
  人脸框、识别结果），也可以把舵机角度或任意文本发下去。

整个程序不依赖 Wi-Fi，插上 OTG 线就能用。
"""

from __future__ import annotations

import json
import queue
import sys
import time
from collections import deque

import cv2
import numpy as np
import serial
from serial.tools import list_ports

from PySide6.QtCharts import QChart, QChartView, QLineSeries, QValueAxis
from PySide6.QtCore import QMargins, QPointF, QRect, QRectF, Qt, QThread, Signal
from PySide6.QtGui import QColor, QFont, QImage, QPainter, QPen
from PySide6.QtWidgets import (
    QApplication,
    QCheckBox,
    QComboBox,
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QPushButton,
    QScrollArea,
    QSizePolicy,
    QSpinBox,
    QSplitter,
    QVBoxLayout,
    QWidget,
)

APP_TITLE = "DeskFriend USB 摄像头"
ESPRESSIF_VID = 0x303A

# 与固件 UVC 描述符广告的四种帧尺寸一致
MODES = [
    ("QVGA 320 × 240", (320, 240)),
    ("HVGA 480 × 320", (480, 320)),
    ("VGA 640 × 480", (640, 480)),
    ("SVGA 800 × 600", (800, 600)),
]

TEMP_WINDOW_S = 180.0  # 3 分钟，与固件网页一致


def camera_backends() -> list[int]:
    # Windows 上只用 DirectShow：MSMF 打开一个被占用的设备会卡十几秒，
    # 而 ESP32 的 UVC 在两者下表现一致。
    if sys.platform.startswith("win"):
        return [cv2.CAP_DSHOW]
    if sys.platform == "darwin":
        return [cv2.CAP_AVFOUNDATION]
    return [cv2.CAP_ANY]


def find_cdc_port() -> str | None:
    """Espressif 复合设备的串口（CDC ACM）。找不到就等用户手动选。"""
    for port in list_ports.comports():
        if port.vid == ESPRESSIF_VID:
            return port.device
    return None


class CameraWorker(QThread):
    frame_ready = Signal(object)
    status = Signal(str)

    # ESP32 独有、笔记本摄像头给不出的模式，用来在自动模式里认出板子。
    PROBE_SIZE = (480, 320)

    def __init__(self, index: int | None = None, size: tuple[int, int] = (320, 240)) -> None:
        super().__init__()
        self._running = True
        self._index = index
        self._size = size
        self._restart = False

    def configure(self, index: int | None = None, size: tuple[int, int] | None = None) -> None:
        if index is not None:
            self._index = index
        if size is not None:
            self._size = size
        self._restart = True

    def use_auto(self) -> None:
        self._index = None
        self._restart = True

    def stop(self) -> None:
        self._running = False

    @staticmethod
    def _prepare(cap: cv2.VideoCapture, size: tuple[int, int]) -> None:
        # MJPG 是唯一走等时传输的格式，也是设备端唯一广告的格式。
        cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, size[0])
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, size[1])

    def _open_at(self, index: int, size: tuple[int, int]) -> cv2.VideoCapture | None:
        for backend in camera_backends():
            if not self._running:
                return None
            cap = cv2.VideoCapture(index, backend)
            if not cap.isOpened():
                cap.release()
                continue
            self._prepare(cap, size)
            ok, _ = cap.read()
            if ok:
                return cap
            cap.release()
        return None

    def _open_auto(self) -> cv2.VideoCapture | None:
        # 自动模式只认 480×320 这个 ESP32 独有的模式：笔记本自带的摄像头给不
        # 出这个尺寸，所以不会被误开（也不会亮灯）。命中后沿用同一个句柄切到
        # 用户选择的尺寸——ESP32 的 UVC 在主机反复关流/重开时容易挂住。
        for index in range(6):
            if not self._running:
                return None
            for backend in camera_backends():
                if not self._running:
                    return None
                cap = cv2.VideoCapture(index, backend)
                if not cap.isOpened():
                    cap.release()
                    continue
                self._prepare(cap, self.PROBE_SIZE)
                ok, frame = cap.read()
                if not ok or frame is None:
                    cap.release()
                    continue
                if (frame.shape[1], frame.shape[0]) != self.PROBE_SIZE:
                    cap.release()
                    continue
                self._index = index
                if self._size != self.PROBE_SIZE:
                    self._prepare(cap, self._size)
                    for _ in range(6):
                        ok, _ = cap.read()
                        if ok:
                            return cap
                        time.sleep(0.15)
                    cap.release()
                    return None
                return cap
        return None

    def _open(self) -> cv2.VideoCapture | None:
        if self._index is not None:
            return self._open_at(self._index, self._size)
        return self._open_auto()

    def run(self) -> None:
        while self._running:
            cap = self._open()
            if cap is None:
                self.status.emit("没找到 ESP32 摄像头（等待 OTG 连接…）")
                time.sleep(1.0)
                continue

            width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
            height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
            self.status.emit(f"摄像头 {self._index}：{width} × {height}")
            self._restart = False

            while self._running and not self._restart:
                ok, frame = cap.read()
                if not ok:
                    self.status.emit("画面中断，正在重连…")
                    break
                self.frame_ready.emit(frame)

            cap.release()
            # 给设备一点时间收尾，别在主机关流后立刻重新提交等时带宽。
            time.sleep(0.5)


class LinkWorker(QThread):
    telemetry = Signal(dict)
    log = Signal(str)
    status = Signal(bool, str)

    def __init__(self, port: str | None = None) -> None:
        super().__init__()
        self._running = True
        self._port = port
        self._outbox: queue.Queue[bytes] = queue.Queue()

    def set_port(self, port: str | None) -> None:
        self._port = port

    def send(self, text: str) -> None:
        self._outbox.put((text.rstrip("\r\n") + "\n").encode("utf-8"))

    def stop(self) -> None:
        self._running = False

    def run(self) -> None:
        ser: serial.Serial | None = None
        while self._running:
            if ser is None:
                port = self._port or find_cdc_port()
                if not port:
                    self.status.emit(False, "未找到 CDC 串口")
                    time.sleep(1.0)
                    continue
                try:
                    ser = serial.Serial(port, 115200, timeout=0.2)
                    self.status.emit(True, f"数据链路 {port}")
                    self.log.emit(f"已连接 {port}")
                except (serial.SerialException, OSError) as exc:
                    self.status.emit(False, str(exc))
                    time.sleep(1.0)
                    continue

            try:
                while not self._outbox.empty():
                    ser.write(self._outbox.get_nowait())
                raw = ser.readline()
            except (serial.SerialException, OSError) as exc:
                self.status.emit(False, f"数据链路断开：{exc}")
                try:
                    ser.close()
                except Exception:
                    pass
                ser = None
                continue

            if not raw:
                continue
            line = raw.decode("utf-8", errors="ignore").strip()
            if not line:
                continue
            if line.startswith("{"):
                try:
                    self.telemetry.emit(json.loads(line))
                except json.JSONDecodeError:
                    self.log.emit(line)
            else:
                self.log.emit(line)

        if ser is not None:
            ser.close()


class VideoWidget(QWidget):
    """等比缩放显示帧，并按帧坐标绘制人脸框。"""

    def __init__(self) -> None:
        super().__init__()
        self.setMinimumSize(320, 180)
        self._frame: np.ndarray | None = None
        self._boxes: list[list[int]] = []
        self._matched = False
        self._show_boxes = True
        self._last_frame_at = 0.0

    def set_frame(self, frame: np.ndarray) -> None:
        self._frame = frame
        self._last_frame_at = time.monotonic()
        self.update()

    def set_overlay(self, boxes: list[list[int]], matched: bool) -> None:
        self._boxes = boxes or []
        self._matched = matched
        self.update()

    def set_show_boxes(self, show: bool) -> None:
        self._show_boxes = show
        self.update()

    def paintEvent(self, event) -> None:  # noqa: N802 (Qt 命名)
        del event
        painter = QPainter(self)
        try:
            painter.fillRect(self.rect(), QColor("#05070a"))
            frame = self._frame
            if frame is None or time.monotonic() - self._last_frame_at > 3.0:
                painter.setPen(QColor("#aab5c2"))
                font = QFont()
                font.setPointSizeF(max(10.0, self.height() / 40.0))
                painter.setFont(font)
                painter.drawText(self.rect(), Qt.AlignCenter, "等待 USB 摄像头画面…")
                return

            height, width = frame.shape[:2]
            scale = min(self.width() / width, self.height() / height)
            draw_w, draw_h = int(width * scale), int(height * scale)
            offset_x = (self.width() - draw_w) // 2
            offset_y = (self.height() - draw_h) // 2

            image = QImage(frame.data, width, height, frame.strides[0], QImage.Format.Format_BGR888)
            painter.drawImage(QRect(offset_x, offset_y, draw_w, draw_h), image)

            if not self._show_boxes or not self._boxes:
                return

            # 线条和字号跟着画面缩放，放大窗口后不会变成细线小字。
            pen_width = max(2, int(round(scale * 2)))
            font = QFont()
            font.setPointSizeF(max(9.0, scale * 13.0))
            painter.setFont(font)
            labelled = False
            for box in self._boxes:
                if len(box) < 4:
                    continue
                x, y, w, h = box[:4]
                hit = self._matched and not labelled
                color = QColor("#8ee6ad" if hit else "#6fb3ff")
                painter.setPen(QPen(color, pen_width))
                painter.drawRect(
                    QRectF(
                        offset_x + x * scale,
                        offset_y + y * scale,
                        w * scale,
                        h * scale,
                    )
                )
                if hit:
                    painter.drawText(
                        QPointF(offset_x + x * scale, offset_y + y * scale - 4),
                        "已识别",
                    )
                    labelled = True
        finally:
            painter.end()


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle(APP_TITLE)
        # 按屏幕可用区域定尺寸并居中：缩放不是 100% 时（Qt 用逻辑像素，屏幕
        # 实际像素更少）固定 1180×820 会把右侧状态栏顶到屏幕外面。
        screen = QApplication.primaryScreen().availableGeometry()
        width = max(860, min(1180, screen.width() - 40))
        height = max(540, min(820, screen.height() - 60))
        self.resize(width, height)
        self.move(screen.x() + (screen.width() - width) // 2,
                  screen.y() + (screen.height() - height) // 2)
        # 1366×768 在 125% 缩放下逻辑高度约 614，窗口必须能塞进去。
        self.setMinimumSize(860, 540)

        self._temps: deque[tuple[float, float]] = deque()
        self._face_on = True

        self.video = VideoWidget()
        self.camera = CameraWorker()
        self.link = LinkWorker()

        self._build_ui()

        self.camera.frame_ready.connect(self.video.set_frame)
        self.camera.status.connect(self._on_camera_status)
        self.link.telemetry.connect(self.on_telemetry)
        self.link.log.connect(self._on_log)
        self.link.status.connect(self._on_link_status)

        self.camera.start()
        self.link.start()

        self._refresh_ports()

    # ---------------- UI ----------------

    def _build_ui(self) -> None:
        central = QWidget()
        self.setCentralWidget(central)
        root = QVBoxLayout(central)
        root.setContentsMargins(10, 10, 10, 10)
        root.setSpacing(10)

        split = QSplitter(Qt.Horizontal)
        split.addWidget(self.video)
        split.addWidget(self._build_side_panel())
        split.setStretchFactor(0, 1)
        split.setStretchFactor(1, 0)
        split.setSizes([820, 330])
        root.addWidget(split, 3)

        root.addWidget(self._build_chart_card(), 2)
        root.addWidget(self._build_control_card(), 0)

    def _row(self, grid: QGridLayout, row: int, title: str) -> QLabel:
        key = QLabel(title)
        key.setObjectName("key")
        value = QLabel("-")
        value.setObjectName("value")
        value.setWordWrap(True)
        # 中文没有空格，wordWrap 会把整句当成一个“单词”，最小宽度等于整句；
        # 允许横向收缩，长错误信息才会在列内换行而不是把面板撑宽。
        value.setMinimumWidth(0)
        value.setSizePolicy(QSizePolicy.Ignored, QSizePolicy.Preferred)
        value.setTextInteractionFlags(Qt.TextSelectableByMouse)
        grid.addWidget(key, row, 0)
        grid.addWidget(value, row, 1)
        return value

    def _build_side_panel(self) -> QWidget:
        # 状态行会被临时错误信息撑长，窗口又可能被用户缩小；放进滚动区，
        # 让内容保持自身高度而不是被挤压、互相重叠。
        content = QWidget()
        layout = QVBoxLayout(content)
        layout.setContentsMargins(0, 0, 8, 0)
        layout.setSpacing(5)

        title = QLabel("设备状态")
        title.setObjectName("cardTitle")
        layout.addWidget(title)

        grid = QGridLayout()
        grid.setColumnStretch(1, 1)
        grid.setVerticalSpacing(4)
        self.lbl_camera = self._row(grid, 0, "摄像头")
        self.lbl_link = self._row(grid, 1, "数据链路")
        self.lbl_res = self._row(grid, 2, "分辨率")
        self.lbl_fps = self._row(grid, 3, "画面帧率")
        self.lbl_temp = self._row(grid, 4, "芯片温度")
        self.lbl_faces = self._row(grid, 5, "人脸")
        self.lbl_identity = self._row(grid, 6, "识别")
        self.lbl_similarity = self._row(grid, 7, "相似度")
        self.lbl_db = self._row(grid, 8, "人脸库")
        layout.addLayout(grid)

        buttons = QHBoxLayout()
        self.btn_face = QPushButton("停止人脸检测")
        self.btn_face.clicked.connect(self._toggle_face)
        buttons.addWidget(self.btn_face)
        layout.addLayout(buttons)

        buttons2 = QHBoxLayout()
        btn_enroll = QPushButton("注册当前人脸")
        btn_enroll.clicked.connect(lambda: self.link.send("ENROLL"))
        btn_clear = QPushButton("清空人脸库")
        btn_clear.setObjectName("secondary")
        btn_clear.clicked.connect(lambda: self.link.send("CLEAR"))
        buttons2.addWidget(btn_enroll)
        buttons2.addWidget(btn_clear)
        layout.addLayout(buttons2)

        self.chk_boxes = QCheckBox("显示人脸框")
        self.chk_boxes.setChecked(True)
        self.chk_boxes.toggled.connect(self.video.set_show_boxes)
        layout.addWidget(self.chk_boxes)

        mode_row = QHBoxLayout()
        mode_row.addWidget(QLabel("分辨率"))
        self.cmb_mode = QComboBox()
        for label, _ in MODES:
            self.cmb_mode.addItem(label)
        self.cmb_mode.currentIndexChanged.connect(self._select_mode)
        mode_row.addWidget(self.cmb_mode, 1)
        layout.addLayout(mode_row)

        cam_row = QHBoxLayout()
        cam_row.addWidget(QLabel("摄像头"))
        self.cmb_cam = QComboBox()
        self.cmb_cam.addItem("自动")
        for index in range(5):
            self.cmb_cam.addItem(str(index))
        self.cmb_cam.currentIndexChanged.connect(self._select_camera)
        cam_row.addWidget(self.cmb_cam, 1)
        layout.addLayout(cam_row)

        ports = QHBoxLayout()
        ports.addWidget(QLabel("串口"))
        self.cmb_port = QComboBox()
        self.cmb_port.setMinimumWidth(120)
        self.cmb_port.currentTextChanged.connect(self._select_port)
        ports.addWidget(self.cmb_port, 1)
        refresh = QPushButton("刷新")
        refresh.setObjectName("secondary")
        refresh.clicked.connect(self._refresh_ports)
        ports.addWidget(refresh)
        layout.addLayout(ports)

        layout.addStretch(1)

        scroll = QScrollArea()
        scroll.setWidget(content)
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        scroll.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        scroll.setMinimumWidth(300)
        scroll.setMaximumWidth(430)
        return scroll

    def _build_chart_card(self) -> QWidget:
        card = QWidget()
        card.setObjectName("card")
        # 曲线是辅助信息，限制高度，把纵向空间留给画面和右侧状态。
        card.setMaximumHeight(210)
        layout = QVBoxLayout(card)
        layout.setContentsMargins(12, 10, 12, 8)

        head = QHBoxLayout()
        title = QLabel("芯片温度")
        title.setObjectName("cardTitle")
        head.addWidget(title)
        head.addStretch(1)
        self.lbl_temp_big = QLabel("-- °C")
        self.lbl_temp_big.setObjectName("tempBig")
        head.addWidget(self.lbl_temp_big)
        layout.addLayout(head)

        self.series = QLineSeries()
        self.series.setColor(QColor("#6fb3ff"))
        pen = QPen(QColor("#6fb3ff"))
        pen.setWidthF(2.0)
        self.series.setPen(pen)

        self.chart = QChart()
        self.chart.addSeries(self.series)
        self.chart.setBackgroundBrush(QColor("#1a212b"))
        self.chart.setPlotAreaBackgroundVisible(False)
        self.chart.legend().hide()
        self.chart.setMargins(QMargins(4, 4, 4, 4))
        self.chart.setAnimationOptions(QChart.AnimationOption.NoAnimation)

        self.axis_x = QValueAxis()
        self.axis_x.setRange(-TEMP_WINDOW_S, 0)
        self.axis_x.setLabelFormat("%.0f")
        self.axis_x.setTitleText("秒前")
        self.axis_y = QValueAxis()
        self.axis_y.setRange(20, 70)
        self.axis_y.setLabelFormat("%.0f")
        self.axis_y.setTitleText("°C")
        for axis in (self.axis_x, self.axis_y):
            axis.setLabelsColor(QColor("#aab5c2"))
            axis.setTitleBrush(QColor("#aab5c2"))
            axis.setGridLineColor(QColor("#2c3745"))
            axis.setLinePenColor(QColor("#2c3745"))
        self.chart.addAxis(self.axis_x, Qt.AlignBottom)
        self.chart.addAxis(self.axis_y, Qt.AlignLeft)
        self.series.attachAxis(self.axis_x)
        self.series.attachAxis(self.axis_y)

        view = QChartView(self.chart)
        view.setRenderHint(QPainter.Antialiasing)
        view.setMinimumHeight(100)
        layout.addWidget(view, 1)
        return card

    def _build_control_card(self) -> QWidget:
        card = QWidget()
        card.setObjectName("card")
        layout = QVBoxLayout(card)
        layout.setContentsMargins(12, 10, 12, 10)
        layout.setSpacing(6)

        servo_row = QHBoxLayout()
        title = QLabel("舵机")
        title.setObjectName("cardTitle")
        servo_row.addWidget(title)
        self.spin_a = QSpinBox()
        self.spin_a.setRange(0, 180)
        self.spin_a.setValue(90)
        self.spin_a.setSuffix("°")
        self.spin_b = QSpinBox()
        self.spin_b.setRange(0, 180)
        self.spin_b.setValue(90)
        self.spin_b.setSuffix("°")
        servo_row.addWidget(QLabel("水平"))
        servo_row.addWidget(self.spin_a)
        servo_row.addWidget(QLabel("俯仰"))
        servo_row.addWidget(self.spin_b)
        btn = QPushButton("发送")
        btn.clicked.connect(self._send_servo)
        servo_row.addWidget(btn)
        servo_row.addStretch(1)
        layout.addLayout(servo_row)

        custom_row = QHBoxLayout()
        title = QLabel("自定义")
        title.setObjectName("cardTitle")
        custom_row.addWidget(title)
        self.edit_custom = QLineEdit()
        self.edit_custom.setPlaceholderText("任意文本，例如 SERVO 90 120 / PING")
        self.edit_custom.returnPressed.connect(self._send_custom)
        custom_row.addWidget(self.edit_custom, 1)
        btn2 = QPushButton("发送")
        btn2.clicked.connect(self._send_custom)
        custom_row.addWidget(btn2)
        layout.addLayout(custom_row)

        self.lbl_log = QLabel("等待数据链路…")
        self.lbl_log.setObjectName("hint")
        self.lbl_log.setTextInteractionFlags(Qt.TextSelectableByMouse)
        layout.addWidget(self.lbl_log)
        return card

    # ---------------- 交互 ----------------

    def _set_value(self, label: QLabel, text: str, warn: bool = False) -> None:
        label.setText(text)
        label.setProperty("warn", warn)
        label.style().unpolish(label)
        label.style().polish(label)

    def _refresh_ports(self) -> None:
        current = self.cmb_port.currentText()
        self.cmb_port.blockSignals(True)
        self.cmb_port.clear()
        self.cmb_port.addItem("自动")
        for port in list_ports.comports():
            label = f"{port.device}"
            self.cmb_port.addItem(label)
        if current:
            index = self.cmb_port.findText(current)
            if index >= 0:
                self.cmb_port.setCurrentIndex(index)
        self.cmb_port.blockSignals(False)

    def _select_port(self, text: str) -> None:
        self.link.set_port(None if text in ("", "自动") else text)

    def _select_mode(self, index: int) -> None:
        if 0 <= index < len(MODES):
            self.camera.configure(size=MODES[index][1])

    def _select_camera(self, index: int) -> None:
        if index <= 0:
            self.camera.use_auto()
        else:
            self.camera.configure(index=index - 1)

    def _toggle_face(self) -> None:
        self._face_on = not self._face_on
        self.link.send(f"FACE {1 if self._face_on else 0}")
        self.btn_face.setText("停止人脸检测" if self._face_on else "启动人脸检测")

    def _send_servo(self) -> None:
        self.link.send(f"SERVO {self.spin_a.value()} {self.spin_b.value()}")

    def _send_custom(self) -> None:
        text = self.edit_custom.text().strip()
        if text:
            self.link.send(text)

    def _on_camera_status(self, text: str) -> None:
        self._set_value(self.lbl_camera, text, text.startswith(("找不到", "画面中断", "摄像头读取失败")))

    def _on_log(self, text: str) -> None:
        self.lbl_log.setText(f"← {text}")

    def _on_link_status(self, connected: bool, text: str) -> None:
        self._set_value(self.lbl_link, text, not connected)

    def on_telemetry(self, data: dict) -> None:
        now = time.monotonic()

        width = data.get("w")
        height = data.get("h")
        if width and height:
            self._set_value(self.lbl_res, f"{width} × {height}")

        fps = data.get("fps")
        if isinstance(fps, (int, float)):
            self._set_value(self.lbl_fps, f"{fps:.1f} fps")

        temperature = data.get("t")
        if isinstance(temperature, (int, float)):
            self._temps.append((now, float(temperature)))
            while self._temps and now - self._temps[0][0] > TEMP_WINDOW_S:
                self._temps.popleft()
            self.lbl_temp_big.setText(f"{temperature:.1f} °C")
            self._set_value(self.lbl_temp, f"{temperature:.1f} °C")
            self._update_chart(now)

        faces = data.get("faces") or []
        self._set_value(self.lbl_faces, f"{len(faces)} 张")

        identity = data.get("id", -1)
        matched = isinstance(identity, int) and identity >= 0
        self._set_value(self.lbl_identity, "已识别" if matched else "未识别", not matched)
        similarity = data.get("sim")
        if isinstance(similarity, (int, float)):
            self._set_value(self.lbl_similarity, f"{similarity:.2f}")

        db = data.get("db")
        if isinstance(db, int):
            self._set_value(self.lbl_db, f"{db} 张")

        self.video.set_overlay(faces, matched)

    def _update_chart(self, now: float) -> None:
        values = [t for t in self._temps if now - t[0] <= TEMP_WINDOW_S]
        points = [QPointF(timestamp - now, value) for timestamp, value in values]
        self.series.replace(points)

        if not values:
            return
        low = min(v for _, v in values)
        high = max(v for _, v in values)
        low = min(low - 1.0, 20.0)
        high = max(high + 1.0, 30.0)
        self.axis_y.setRange(low, high)

    def closeEvent(self, event) -> None:  # noqa: N802
        self.camera.stop()
        self.link.stop()
        self.camera.wait(5000)
        self.link.wait(2000)
        # 若线程仍卡在驱动里（拔线/相机无响应），退出前强制收尾，避免 Qt
        # 在析构仍在运行的 QThread 时直接崩溃。
        if self.camera.isRunning():
            self.camera.terminate()
            self.camera.wait(500)
        super().closeEvent(event)


STYLESHEET = """
QWidget { background: #10141b; color: #eef2f7;
          font-family: system-ui, -apple-system, "Segoe UI", "Microsoft YaHei", sans-serif;
          font-size: 10pt; }
QWidget#card { background: #1a212b; border: 1px solid #2c3745; border-radius: 12px; }
QLabel#cardTitle { font-weight: 600; font-size: 11pt; }
QLabel#key { color: #aab5c2; }
QLabel#value { color: #8ee6ad; font-variant-numeric: tabular-nums; }
QLabel#value[warn="true"] { color: #f0b4b4; }
QLabel#tempBig { color: #6fb3ff; font-size: 18pt; font-weight: 600; }
QLabel#hint { color: #aab5c2; }
QPushButton { background: #3d82f6; color: white; border: 0; border-radius: 7px;
              padding: 8px 12px; }
QPushButton:hover { background: #5a96f8; }
QPushButton#secondary { background: #485464; }
QComboBox, QSpinBox, QLineEdit { background: #222b36; border: 1px solid #2c3745;
                                 border-radius: 6px; padding: 5px 8px; }
QSplitter::handle { background: transparent; }
"""


def main() -> int:
    QApplication.setHighDpiScaleFactorRoundingPolicy(
        Qt.HighDpiScaleFactorRoundingPolicy.PassThrough
    )
    app = QApplication(sys.argv)
    app.setApplicationName(APP_TITLE)
    app.setStyleSheet(STYLESHEET)
    window = MainWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
