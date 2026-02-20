"""
Animación: Onda EM Plana - Tema 2.2
Campos y Ondas Electromagnéticos | Equipo 1 | 3EM24

Para ejecutar:
  pip install manim
  manim -pql onda_plana_manim.py OndaEMPlana

Para alta calidad:
  manim -pqh onda_plana_manim.py OndaEMPlana
"""

from manim import *
import numpy as np

class OndaEMPlana(ThreeDScene):
    def construct(self):
        # ── Configuración de cámara 3D ──
        self.set_camera_orientation(phi=65*DEGREES, theta=-45*DEGREES)
        self.camera.set_zoom(0.85)

        # ── Título ──
        titulo = Text("Onda Electromagnética Plana", font_size=28, color=YELLOW)
        subtitulo = Text("Tema 2.2 | E ⊥ B ⊥ k̂", font_size=18, color=WHITE)
        titulo.to_edge(UP).shift(LEFT*1)
        subtitulo.next_to(titulo, DOWN, buff=0.1)
        self.add_fixed_in_frame_mobjects(titulo, subtitulo)
        self.play(Write(titulo), Write(subtitulo), run_time=1.5)

        # ── Ejes 3D ──
        axes = ThreeDAxes(
            x_range=[-0.5, 7, 1],
            y_range=[-2, 2, 1],
            z_range=[-2, 2, 1],
            x_length=7,
            y_length=4,
            z_length=4,
            axis_config={"color": GREY, "stroke_width": 2},
        )

        # Etiquetas de ejes
        label_z = axes.get_z_axis_label(MathTex("z", color=WHITE), edge=RIGHT)
        label_x = axes.get_x_axis_label(
            MathTex("\\hat{k}", color=YELLOW, font_size=36), edge=RIGHT
        )
        label_E = Text("E (campo eléctrico)", font_size=14, color=BLUE)
        label_B = Text("B (campo magnético)", font_size=14, color=RED)
        label_E.to_corner(DL).shift(UP*0.5)
        label_B.next_to(label_E, DOWN, buff=0.1)
        self.add_fixed_in_frame_mobjects(label_E, label_B)

        self.play(Create(axes), Write(label_z), Write(label_x), run_time=2)
        self.play(Write(label_E), Write(label_B))

        # ── Parámetros de la onda ──
        n_points = 300
        x_vals = np.linspace(0, 6.5, n_points)
        amplitude = 1.5
        k = 1.5  # número de onda (visual)

        # ── Campo E (oscila en eje Y - azul) ──
        def campo_E(t_offset=0):
            points = []
            for x in x_vals:
                y = amplitude * np.sin(k * x - t_offset)
                points.append(axes.c2p(x, y, 0))
            return VMobject().set_points_as_corners(points).set_color(BLUE).set_stroke(width=3)

        # ── Campo B (oscila en eje Z - rojo) ──
        def campo_B(t_offset=0):
            points = []
            for x in x_vals:
                z = amplitude * np.sin(k * x - t_offset)
                points.append(axes.c2p(x, 0, z))
            return VMobject().set_points_as_corners(points).set_color(RED).set_stroke(width=3)

        # ── Flechas representativas E y B ──
        def flechas_E(t_offset=0):
            arrows = VGroup()
            for x in np.linspace(0.3, 6.3, 12):
                y = amplitude * np.sin(k * x - t_offset)
                if abs(y) > 0.1:
                    arr = Arrow3D(
                        start=axes.c2p(x, 0, 0),
                        end=axes.c2p(x, y, 0),
                        color=BLUE,
                        resolution=4,
                        thickness=0.02,
                    )
                    arrows.add(arr)
            return arrows

        def flechas_B(t_offset=0):
            arrows = VGroup()
            for x in np.linspace(0.3, 6.3, 12):
                z = amplitude * np.sin(k * x - t_offset)
                if abs(z) > 0.1:
                    arr = Arrow3D(
                        start=axes.c2p(x, 0, 0),
                        end=axes.c2p(x, 0, z),
                        color=RED,
                        resolution=4,
                        thickness=0.02,
                    )
                    arrows.add(arr)
            return arrows

        # ── Dibujar ondas iniciales ──
        curva_E = campo_E(0)
        curva_B = campo_B(0)
        arr_E = flechas_E(0)
        arr_B = flechas_B(0)

        self.play(Create(curva_E), Create(curva_B), run_time=2)
        self.play(Create(arr_E), Create(arr_B), run_time=1.5)

        # ── Ecuación en pantalla ──
        eq1 = MathTex(
            r"\mathbf{E}(z,t) = E_0 \cos(\omega t - \beta z)\,\hat{x}",
            font_size=22, color=BLUE
        )
        eq2 = MathTex(
            r"\mathbf{H}(z,t) = H_0 \cos(\omega t - \beta z)\,\hat{y}",
            font_size=22, color=RED
        )
        eq1.to_corner(DR).shift(UP*1.2)
        eq2.next_to(eq1, DOWN, buff=0.2)
        self.add_fixed_in_frame_mobjects(eq1, eq2)
        self.play(Write(eq1), Write(eq2), run_time=1.5)

        # ── Rotación de cámara inicial ──
        self.begin_ambient_camera_rotation(rate=0.15)
        self.wait(2)
        self.stop_ambient_camera_rotation()

        # ── Animación de propagación ──
        aviso = Text("Propagación en dirección +z →", font_size=16, color=YELLOW)
        aviso.to_edge(DOWN)
        self.add_fixed_in_frame_mobjects(aviso)
        self.play(Write(aviso))

        # Animar 2 ciclos completos
        n_frames = 40
        for i in range(1, n_frames + 1):
            t = (i / n_frames) * 4 * PI
            nueva_E = campo_E(t)
            nueva_B = campo_B(t)
            nuevas_arrE = flechas_E(t)
            nuevas_arrB = flechas_B(t)
            self.play(
                Transform(curva_E, nueva_E),
                Transform(curva_B, nueva_B),
                Transform(arr_E, nuevas_arrE),
                Transform(arr_B, nuevas_arrB),
                run_time=0.08,
                rate_func=linear
            )

        # ── Condición TEM destacada ──
        self.play(FadeOut(aviso))
        tem = MathTex(
            r"\mathbf{E} \perp \mathbf{B} \perp \hat{k}",
            font_size=32, color=YELLOW
        )
        tem_label = Text("Condición TEM", font_size=20, color=YELLOW)
        tem.to_edge(DOWN).shift(UP*0.3)
        tem_label.next_to(tem, DOWN, buff=0.1)
        self.add_fixed_in_frame_mobjects(tem, tem_label)
        self.play(Write(tem), Write(tem_label), run_time=1.5)

        # Rotación final lenta para apreciar la geometría
        self.begin_ambient_camera_rotation(rate=0.2)
        self.wait(4)
        self.stop_ambient_camera_rotation()

        # ── Fade out final ──
        self.play(
            FadeOut(curva_E), FadeOut(curva_B),
            FadeOut(arr_E), FadeOut(arr_B),
            run_time=1.5
        )

        creditos = Text("Equipo 1 | 3EM24 | Campos y Ondas EM", font_size=16, color=GREY)
        creditos.to_edge(DOWN)
        self.add_fixed_in_frame_mobjects(creditos)
        self.play(Write(creditos))
        self.wait(1.5)


# ─── BONUS: Escena de onda esférica (más simple) ───
class OndaEsferica(Scene):
    def construct(self):
        titulo = Text("Onda Electromagnética Esférica", font_size=28, color=YELLOW)
        subtitulo = Text("Tema 2.2 — Amplitud ∝ 1/r", font_size=18, color=WHITE)
        titulo.to_edge(UP)
        subtitulo.next_to(titulo, DOWN, buff=0.1)
        self.play(Write(titulo), Write(subtitulo))

        # Fuente puntual
        fuente = Dot(ORIGIN, color=YELLOW, radius=0.15)
        label_fuente = Text("fuente", font_size=14, color=YELLOW).next_to(fuente, DOWN, buff=0.15)
        self.play(Create(fuente), Write(label_fuente))

        # Frentes de onda esféricos expandiéndose
        colores = [BLUE, TEAL, GREEN, YELLOW_E, ORANGE]
        radios_max = [1.0, 1.8, 2.6, 3.4, 4.2]

        circulos = []
        for i, (r_max, color) in enumerate(zip(radios_max, colores)):
            circulo = Circle(radius=0.05, color=color, stroke_width=2.5)
            circulos.append(circulo)

        self.play(*[Create(c) for c in circulos])

        # Animar expansión con amplitud decreciente
        animaciones = []
        for i, (c, r_max, color) in enumerate(zip(circulos, radios_max, colores)):
            # Opacidad decrece con la distancia (1/r)
            opacidad_final = max(0.15, 1.0 / (i + 1))
            animaciones.append(
                c.animate.become(
                    Circle(radius=r_max, color=color, stroke_width=2.5)
                    .set_stroke(opacity=opacidad_final)
                )
            )

        self.play(*animaciones, run_time=3, rate_func=smooth)

        # Etiqueta 1/r
        label_r = MathTex(r"|\mathbf{E}| \propto \frac{1}{r}", font_size=30, color=WHITE)
        label_r.to_corner(DR).shift(UP)
        self.play(Write(label_r))

        # Comparación con onda plana a gran distancia
        nota = Text("A gran distancia → comportamiento como onda plana", 
                    font_size=14, color=GREY)
        nota.to_edge(DOWN)
        self.play(Write(nota))
        self.wait(2)

        self.play(
            *[FadeOut(c) for c in circulos],
            FadeOut(fuente), FadeOut(label_fuente),
            FadeOut(label_r), FadeOut(nota),
            run_time=1.5
        )
        creditos = Text("Equipo 1 | 3EM24 | Campos y Ondas EM", font_size=14, color=GREY)
        creditos.to_edge(DOWN)
        self.play(Write(creditos))
        self.wait(1)
