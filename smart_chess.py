"""
*Name: Smart Chess (selfmoving chessboard) 
Author: Filip Momot

This program creates a graphical chess interface using Tkinter,
sends move information to the ESP32 through sockets, and uses Stockfish
to make the best moves. Voice moves are handled using
speech recognition library.
"""

import tkinter as tk
import chess
from stockfish import Stockfish
import socket
import threading
import speech_recognition as sr

#path to stcokfish file
stockfish_path = "C:/Users/filip.momot/Desktop/Tillämpad pro/smart_chess/stockfish-windows-x86-64-avx2/stockfish/stockfish-windows-x86-64-avx2.exe"

sf = Stockfish(path=stockfish_path)

HOST = "0.0.0.0"
PORT = 10000

# This class creates the chess GUI and handles
# communication between Stockfish and the ESP32
class ChessGUI:
    #this function creates the board and buttons around it
    def __init__(self, root):
        self.root = root
        self.root.title("Stockfish TP Projekt")

        self.speech_button = tk.Button(root, text="Tala", command=self.listen_for_move)
        self.speech_button.pack()

        self.board = chess.Board()
        self.conn = None  

        self.label = tk.Label(root, text=self.board.unicode(), font=("Courier", 47))
        self.label.pack()

        self.status_label = tk.Label(root, text="Väntar på ESP...", font=("Courier", 12))
        self.status_label.pack()

        self.entry = tk.Entry(root)
        self.entry.pack()

        self.move_button = tk.Button(root, text="Rör dig", command=self.make_move)
        self.move_button.pack()

        self.best_move_button = tk.Button(root, text="Stockfish drag", command=self.stockfish_move)
        self.best_move_button.pack()

        self.update_board()
        threading.Thread(target=self.socket_server, daemon=True).start()

    #updates the board 
    def update_board(self):
        self.label.config(text=self.board.unicode())

    #upadates status text shown in the chess GUI
    def set_status(self, text):
        self.status_label.config(text=text)

    #move (input) from the text field 
    def make_move(self):
        move_text = self.entry.get()
        self.play_move(move_text)
        self.entry.delete(0, tk.END)

    def listen_for_move(self):
        threading.Thread(target=self._listen_thread, daemon=True).start()

    #This function takes in move inforamtion 
    #by convert speech to text using speech_recognition library
    def _listen_thread(self):
        recognizer = sr.Recognizer()
        mic = sr.Microphone()

        self.root.after(0, lambda: self.set_status("Lyssnar på rösten"))

        try:
            with mic as source:
                recognizer.adjust_for_ambient_noise(source, duration=1)
                print("tala")
                audio = recognizer.listen(source, phrase_time_limit = 8)

            text = recognizer.recognize_google(audio, language="en-US").lower() #swapcase(), skulle funka???
            print("hörde", text)

            move = text.replace(" ", "")

            if len(move) == 4: #checks if the move string is 4 chacracters otherwise you need to try again
                self.root.after(0, lambda m=move: self.play_move(m))
            else:
                self.root.after(0, lambda: self.set_status(f"Förstod inte: {text}"))
        
        except sr.UnknownValueError:
            self.root.after(0, lambda: self.set_status("Kunde inte höra, försök igen"))

    #Apply player move, then get and send Stockfish response.
    def play_move(self, move_text):
        try:
            move = chess.Move.from_uci(move_text)

            if move not in self.board.legal_moves:
                print("Ogiltigt drag:", move_text)
                self.send_to_esp("ILLEGAL")
                return

            self.board.push(move)
            self.root.after(0, self.update_board)
            sf.set_fen_position(self.board.fen())

            if self.board.is_game_over():
                print("Spelet är slut!")
                self.root.after(0, lambda: self.set_status("Spelet är slut!"))
                self.send_to_esp("GAME_OVER")
                return

            best_move = sf.get_best_move()
            if best_move:
                self.board.push(chess.Move.from_uci(best_move))
                self.root.after(0, self.update_board)
                self.root.after(0, lambda: self.set_status(f"Stockfish spelade: {best_move}"))
                print("Stockfish drag:", best_move)
                self.send_to_esp(best_move)

        except Exception as e:
            print("Fel:", e)
            self.send_to_esp("ERROR")

    #Manual Stockfish move button (for testing without ESP).
    def stockfish_move(self):        
        sf.set_fen_position(self.board.fen())
        best_move = sf.get_best_move()
        if best_move:
            self.board.push(chess.Move.from_uci(best_move))
            self.update_board()

    #sends message with the move to esp32 
    def send_to_esp(self, message):
        if self.conn:
            try:
                self.conn.sendall((message + "\n").encode())
                print("Skickade till ESP:", message)
            except Exception as e:
                print("Kunde inte skicka till ESP:", e)
                self.conn = None

    #connects to esp32 and recive the moves from it
    def socket_server(self):
        server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server.bind((HOST, PORT))
        server.listen()

        print(f"Lyssnar på port {PORT}...")

        while True:
            conn, addr = server.accept()
            self.conn = conn
            print("ESP32 ansluten:", addr)
            self.root.after(0, lambda: self.set_status(f"ESP32 ansluten: {addr[0]}"))

            while True:
                try:
                    data = conn.recv(1024).decode().strip()
                    if not data:
                        continue  
                    print("Drag från ESP:", data)
                    self.root.after(0, lambda m=data: self.play_move(m))
                except Exception as e:
                    print("Anslutning bruten:", e)
                    break  

            self.conn = None
            self.root.after(0, lambda: self.set_status("ESP32 frånkopplad. Väntar..."))

    def close(self):
        print("Stänger programmet")
        self.root.destroy()


root = tk.Tk()
app = ChessGUI(root)
root.protocol("WM_DELETE_WINDOW", app.close)
root.mainloop()