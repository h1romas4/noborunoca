# -*- coding: utf-8 -*-
#
# lcconv.py
#
# LovelyComposerのデータをMSX用のASMソース形式に変換する。
#
# 仕様：
# - LCのjsonデータから、ChA～Cのデータを対象として処理する。ChD、コードのデータは無視する。
# - speed値を1/2として、1ノートの音長とする(1未満=1とする)
# - トーン('n')
#   - o4aはlc＝69,ドライバのデータ=45なので、以下で計算。
#       value - 24
#     ※MSXではo8まで指定可能だが、lcではo7までになる
#   - 連続して同じトーンが出てきても、別データとして生成する。
#       例) T150でCDEE → c,8,d,8,e,8,e,8 のイメージ(実際は音階はテーブルのidx値)
#   - トーンが無い(=none)場合は、音階0、ボリューム0で音長のみ設定したデータとする
#   - LCSoundの要素(32個)全てnoneのデータが出てきたら、そこでチャネルの処理を終了する(255)
#   - 音長は以下で計算する。
#       (speed / 2) (端数切捨) ※ここは結構影響が大きいので、後で要調整
# - ボリューム('x')
#   - 12を15、1を1とする。以下で計算。
#       value*1.25 (端数切捨)
#   - 直前のデータと値が変わらない場合はデータを出力しない。
# - PSGR#7 (ノイズ/トーンのミキシング)
#   - 以下の音色はノイズとし、以外をトーンとする
#       id=4
#   - 直前のトーンが同じ(ノイズ→ノイズ、トーン→トーン)場合は変更不要とする
#   - LCの音色でノイズとトーンを同時に出すものがないので（だよね？）、常にどちらかとなる。
# - PSGR#6 (ノイズトーン)
#   - ノイズの場合に対象のトーンにより決定、以下で計算する。([低]MSX32/lc24～[高]MSX0/lc107)
#       (107 - value) / 2.59 (端数切捨)
# - ハードウェアエンベロープは使わない。(複数チャンネルで波形が同じになるため、使いにくい)
# - ファイル出力はLCSoundデータ単位に行う(=LCVoiceの要素、32トーン)
import json
import os

#lcc = lcconv()
#lcc.execute(argv)
#sys.exit(0)

#class lcconv:
'''
lcconvクラス
'''
# 出力データクラス
dc = None

#def __init__(self):
#    '''
#    クラス初期化
#    '''

def execute(argv):
    '''
    変換処理実行
    '''
    # 出力データクラスを初期化
    dc = dataClass(argv[1])

    # 変換処理実行
    dc.convert()

    # 出力データクラスからファイルに出力
    dc.export()

class dataClass:
    '''
    出力データクラス
    '''
    # jsonデータのリスト
    data_header = []
    data_body = []

    # ダンプデータ
    dumpData = [0, 0, 0]

    # ベースとなるspeed値
    speed = 0

    # 出力ファイル名
    outFileName = ""

    # 各値の退避変数を初期化
    svVoice = None
    svNote = None
    svVolume = None
    svNoiseTone = None
    svMixing = None

    def __init__(self, jsonFileName:str = ""):
        '''
        初期化処理
        '''
        # 引数のjsonFileNameのjsonファイルを読み込み
        filePath = os.path.normpath(os.path.join(os.path.dirname(__file__), jsonFileName))
        with open(filePath) as f:
            df_header = f.readline()
            df_body = f.readline()

        # 出力ファイル名を設定
        self.outFileName = os.path.splitext(os.path.basename(jsonFileName))[0]
        print("Convert [" + jsonFileName + "] to [" + self.outFileName + ".asm]")

        # jsonデータをパース
        self.data_header = json.loads(df_header)
        self.data_body = json.loads(df_body)

    def convert(self):
        '''
        変換処理
        '''
        # speed値取得
        self.speed = int((self.data_body["speed"]) / 1.8)
        print("speed = " + str(self.speed))

        # 各チャネルのデータを取得
        channels = (self.data_body["channels"])["channels"]
        channelList = list(channels)

        totalSize = 0

        # チャネル1～3に大してダンプデータ作成
        for i in range(3):
            self.dumpData[i] = self.makeDumpData(channelList[i])
            print("track" + str(i+1) + ":" + str(len(self.dumpData[i])) + "bytes.")
            totalSize += len(self.dumpData[i])

        print("total :" + str(totalSize) + "bytes.")

    def makeDumpData(self, argData):
        '''
        ダンプデータ作成処理
        '''

        # 空データカウント
        noneCount = 0

        # 終了判定フラグ
        isTerminate = False

        # 各値の退避変数を初期化
        self.svVoice = None
        self.svNote = None
        self.svVolume = None
        self.svNoiseTone = None
        self.svMixing = None

        # 音長
        time = 0

        sv = {}

        # データバッファ
        buffer = []

        # 'sl'要素を取り出す
        sl = argData["sl"]

        # sl要素の全てに対して繰り返す(0～15)
        for vl in sl:

            sv = vl["vl"][0]
            # ver1.2.0未満のデータ対応
            # sv["x"]が存在しなければ固定値で追加する
            if "x" not in sv:
                sv["x"] = 12

            #音長をリセット
            time = 0

            # vl要素の全てに対して繰り返す(0～31)
            for v in vl["vl"]:

                # ver1.2.0未満のデータ対応
                # v["x"]が存在しなければ固定値で追加する
                if "x" not in v:
                    v["x"] = 12

                # voice,tone,volumeのいずれかが直前のデータと違っていたら、退避していaddBufferを呼ぶ
                # ただし一番最初のデータ時は直前の値が全てNoneなので何もしない
                if v["n"] != sv["n"] or v["id"] != sv["id"] or v["x"] != sv["x"]:
                    buffer += self.addBuffer(sv, time)

                    # データを退避
                    sv = v

                    #音長をリセット
                    time = 0

                # 音長をカウント
                time += self.speed

                # トーン=None and ボリューム値=0の場合は、noneCountをインクリメント、以外はリセット
                if (v["n"] is None):
                    noneCount += 1
                else:
                    noneCount = 0

                # NoneCout=32ならループ終了
                if noneCount == 32:
                    isTerminate = True
                    break

            if isTerminate == False:
                # @ToDo : ここまでのボリューム値の合計を求めて、0の時は追加しないようする
                buffer += self.addBuffer(v, time)
            else:
                break

        return buffer

    def addBuffer(self, vl, time):
        '''
        バッファ追加処理\n
        引数の値からbufferにデータを追加する。\n
        ただし、前回の設定値から変わっていないパラメータは設定しない。\n
        追加するのは以下順とする。\n
        1) mixing\n
        2) volme\n
        3) note or noiseTone\n
        '''
        dataList = []

        # voiceが前回の設定値から変わったか判定する
        # 変わった場合はコマンドとPSGR#6,#7の設定値をバッファに出力する
        if self.svVoice != vl["id"] and vl["id"] != None:
            mixing = self.getMixingValue(vl["id"])
            if mixing != self.svMixing:
                # ミキシングの値が変わった場合のみバッファに出力する
                dataList += ["201", mixing]
                self.svMixing = mixing
                self.svVoice = vl["id"]

        # volumeが前回の設定値から変わったか判定する
        # 変わった場合はコマンドとPSGR#8〜10に設定値をバッファに出力する
        if vl["n"] == None:
            # 一度noteを置いて削除した場合、noteがNoneでもvolumeが設定されているため、対処する
            vl["x"] = 0
        if self.svVolume != vl["x"]:
            dataList += ["200", str(self.getVolumeValue(vl["x"]))]
            self.svVolume = vl["x"]

        # noteは無条件でバッファに出力する
        if self.svMixing == "%10":
            # トーンの時の処理
            dataList += [str(self.getNoteValue(vl["n"])), str(time)]
        else:
            # ノイズの時の処理
            noiseTone = self.getNoiseToneValue(vl["n"])
            if noiseTone != self.svNoiseTone:
                dataList += ["202", str(noiseTone)]
                self.svNoiseTone = noiseTone
            dataList += [str(self.getNoteValue(vl["n"])), str(time)]

        return dataList


    def getNoteValue(self, tone:int) -> int:
        '''
        トーン値取得処理
        '''
        return (tone - 24 if tone != None else 0)

    def getNoiseToneValue(self, tone:int) -> int:
        '''
        ノイズトーン値取得処理
        '''
        return int((107-int(tone if tone != None else 1))/2.59)

    def getMixingValue(self, voice:int) -> str:
        '''
        ミキシング値取得処理
        '''
        return ("%01" if voice == 3 else "%10")

    def getVolumeValue(self, volume:int) -> int:
        '''
        ボリューム値取得処理
        '''
        return int(volume*1.25)

    def export(self):
        '''
        ファイルエクスポート処理
        '''
        # 出力ファイル名
        outFilePath = os.path.normpath(os.path.join(os.path.dirname(__file__), self.outFileName + ".asm"))
        # 出力ラベル名
        outputLabel = "MUSIC_{:s}".format(self.outFileName.upper())

        with open(outFilePath, mode="w") as f:
            # ヘッダー情報
            f.write("_" + outputLabel + ":\n")
            f.write("    DB  0\n")
            for idx in range(3):
                if len(self.dumpData[idx]) > 0:
                    f.write("    DW  _" + outputLabel + "_TRK" + str(idx+1) + "\n")
                else:
                    f.write("    DW  $0000\n")

            # 各チャンネルのデータ
            for idx, ch in enumerate(self.dumpData):
                if len(ch) == 0:
                    break
                else:
                    f.write("_" + outputLabel + "_TRK" + str(idx+1) + ":\n")
                    s = ""
                    for i, v in enumerate(ch):
                        if i % 16 == 0:
                            if s != "":
                                f.write("    DB  " + s + "\n")
                            s =  "{:3}".format(v)
                        else:
                            s += ", " + "{:3}".format(v)
                    if s != "":
                        f.write("    DB  " + s + "\n")
                    # @ToDo : ループ指定の有無で254(ループあり)、255(ループなし)を指定可能としたい
                    f.write("    DB  255\n")

        print("export complete.")

if __name__ == "__main__":
    '''
    アプリケーション実行
    '''
#    execute(["", "00.jsonl"])

    import sys
    execute(sys.argv)
