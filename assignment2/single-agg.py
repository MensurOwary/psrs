import glob
import statistics
import pandas as pd

SINGLE_RUN_TIMES = {
    1000000: 253416,
    16000000: 5380319,
    32000000: 11254041,
    64000000: 22495273,
    128000000: 47589716,
    256000000: 100004774
}

def create_real_times_table(final_results: dict):
    total_times = dict()
    total_times[1] = SINGLE_RUN_TIMES

    for i in range(2, 9, 2):
        total_times[i] = dict()

    for p_count, results in final_results.items():
        for size, value in results.items():
            total_times[p_count][size] = value

    df = pd.DataFrame(total_times).transpose()
    df = df.reindex(sorted(df.columns), axis=1)
    return df


RESULTS = {1: {}, 2: {}, 4: {}, 6: {}, 8: {}}

for file in glob.glob(r"./data_p2p/*.txt", recursive=True):
    with open(file) as f:
        _, pc, size = file[:-4].split("-")
        pc = int(pc)
        size = int(size)
        avg = statistics.mean(map(lambda line: int(
            line.split()[-2]), filter(lambda line: "Complete" in line, f)))
        print(pc, size, avg)
        RESULTS[pc][size] = avg

pd.options.display.float_format = '{:.0f}'.format

real_times_df = create_real_times_table(RESULTS)

print(real_times_df.to_latex())

# w = pd.ExcelWriter("./results/data_new_p2p.xlsx")

# real_times_df.to_excel(w, sheet_name="Real times")

# w.save()