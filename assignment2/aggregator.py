import statistics
import json
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

SINGLE_RUN_TIMES = {
    1000000: 253416,
    16000000: 5380319,
    32000000: 11254041,
    64000000: 22495273,
    128000000: 47589716,
    256000000: 100004774
}

def stat_times(lines):
    values = list(map(lambda line: int(line.split()[-2]), lines))
    return statistics.mean(values), statistics.median(values)

def stat_to_secs(mean_ms, median_ms):
    mean_s = mean_ms / 1000000
    median_s = median_ms / 1000000
    return round(mean_s, ndigits=4), round(median_s, ndigits=4)

def rdfa(lines, size, t):
    return round(max(map(lambda line: int(line.split()[-2]), lines)) / (size / t), ndigits=6)

class SingleResult:
    def __init__(self, 
                    phase0_line, 
                    phase1_lines, 
                    phase2_lines, 
                    phase3_lines, 
                    phase4_lines,
                    phase4_merge_count_lines,
                    phase_merge_line,
                    complete_execution_line,
                    size, 
                    t):
        self.phase0_times = int(phase0_line.split()[-2])
        self.phase1_times = stat_times(phase1_lines)
        self.phase2_times = stat_times(phase2_lines)
        self.phase3_times = stat_times(phase3_lines)
        self.phase4_times = stat_times(phase4_lines)
        self.phase_merge_times = int(phase_merge_line.split()[-2])
        self.rdfa = rdfa(phase4_merge_count_lines, size, t)
        self.total_time = int(complete_execution_line.split()[-2])
        self.size = size
        self.process_count = t
    
    def print(self):
        print("==== Single Result for %d - %d ====" % (self.size, self.process_count))
        print("Phase 0:\t\t", self.phase0_times)
        print("Phase 1:\t\t", self.phase1_times)
        print("Phase 2:\t\t", self.phase2_times)
        print("Phase 3:\t\t", self.phase3_times)
        print("Phase 4:\t\t", self.phase4_times)
        print("Phase Merge:\t\t", self.phase_merge_times)
        print("RDFA:\t\t\t", self.rdfa)
        print("Total time:\t\t", self.total_time)



def calculate_single_iteration(filename):
    phase0_line = None
    phase1_lines = []
    phase2_lines = []
    phase3_lines = []
    phase4_lines = []
    phase4_merge_count_lines = []
    phase_merge_line = None
    complete_execution_line = None

    pn, size, _ = filename.split("out-")[1][:-4].split("-")
    process_count = int(pn)
    size = int(size)

    with open(filename) as file:
        for line in file:
            if line.startswith("[mpi"):
                line = "[" + line[6:]
            line = line.strip()

            if "Phase 0" in line:
                phase0_line = line
            elif "Phase 1" in line:
                phase1_lines.append(line)
            elif "Phase 2" in line:
                phase2_lines.append(line)
            elif "Phase 3" in line:
                phase3_lines.append(line)
            elif "Phase 4" in line:
                phase4_lines.append(line)
            elif "merged" in line:
                phase4_merge_count_lines.append(line)
            elif "Merge" in line:
                phase_merge_line = line
            elif "Complete" in line:
                complete_execution_line = line

    return SingleResult(phase0_line, phase1_lines, phase2_lines, phase3_lines, phase4_lines, phase4_merge_count_lines, phase_merge_line, complete_execution_line, size, process_count)


def create_real_times_table(final_results: dict):
    total_times = dict()
    total_times[1] = SINGLE_RUN_TIMES

    for i in range(2, 9, 2):
        total_times[i] = dict()

    for p_count, results in final_results.items():
        for size, values in results.items():
            total_times[p_count][size] = values[6]
    
    df = pd.DataFrame(total_times).transpose()
    df = df.reindex(sorted(df.columns), axis=1)
    return df

def create_speedup_table(real_times_df):
    df = real_times_df.rdiv(real_times_df.loc[1])
    df['Linear'] = [1, 2, 4, 6, 8] 
    return df

def create_rdfa_table(final_results: dict):
    total_times = dict()

    for i in range(2, 9, 2):
        total_times[i] = dict()

    for p_count, results in final_results.items():
        for size, values in results.items():
            total_times[p_count][size] = values[-1]
    
    df = pd.DataFrame(total_times).transpose()
    df = df.reindex(sorted(df.columns), axis=1)
    return df

def create_phase_by_phase_table(final_results: dict):
    final = {
        "Phase 0": {2: {}, 4: {}, 6: {}, 8: {}},
        "Phase 1": {2: {}, 4: {}, 6: {}, 8: {}},
        "Phase 2": {2: {}, 4: {}, 6: {}, 8: {}},
        "Phase 3": {2: {}, 4: {}, 6: {}, 8: {}},
        "Phase 4": {2: {}, 4: {}, 6: {}, 8: {}},
        "Phase Merge": {2: {}, 4: {}, 6: {}, 8: {}}
    }

    for p_count, results in final_results.items():
        for size, values in results.items():
            final['Phase 0'][p_count][size] = values[0]
            final['Phase 1'][p_count][size] = values[1]
            final['Phase 2'][p_count][size] = values[2]
            final['Phase 3'][p_count][size] = values[3]
            final['Phase 4'][p_count][size] = values[4]
            final['Phase Merge'][p_count][size] = values[5]
    
    p0 = pd.DataFrame(final['Phase 0']).transpose()
    p0 = p0.reindex(sorted(p0.columns), axis=1)

    p1 = pd.DataFrame(final['Phase 1']).transpose()
    p1 = p1.reindex(sorted(p1.columns), axis=1)

    p2 = pd.DataFrame(final['Phase 2']).transpose()
    p2 = p2.reindex(sorted(p2.columns), axis=1)

    p3 = pd.DataFrame(final['Phase 3']).transpose()
    p3 = p3.reindex(sorted(p3.columns), axis=1)

    p4 = pd.DataFrame(final['Phase 4']).transpose()
    p4 = p4.reindex(sorted(p4.columns), axis=1)

    pMerge = pd.DataFrame(final['Phase Merge']).transpose()
    pMerge = pMerge.reindex(sorted(pMerge.columns), axis=1)

    df = pd.concat([p0, p1, p2, p3, p4, pMerge], keys=["Phase 0", "Phase 1", "Phase 2", "Phase 3", "Phase 4", "Phase Merge"])
    return df

def create_phase_by_phase_table_new(final_results: dict):
    final = {
        "Phase 0": {2: {}, 4: {}, 6: {}, 8: {}},
        "Phase 1": {2: {}, 4: {}, 6: {}, 8: {}},
        "Phase 2": {2: {}, 4: {}, 6: {}, 8: {}},
        "Phase 3": {2: {}, 4: {}, 6: {}, 8: {}},
        "Phase 4": {2: {}, 4: {}, 6: {}, 8: {}},
        "Phase Merge": {2: {}, 4: {}, 6: {}, 8: {}}
    }

    final = {
        1000000: {2: {}, 4: {}, 8: {}},
        16000000: {2: {}, 4: {}, 8: {}},
        32000000: {2: {}, 4: {}, 8: {}},
        64000000: {2: {}, 4: {}, 8: {}},
        128000000: {2: {}, 4: {}, 8: {}},
        256000000: {2: {}, 4: {}, 8: {}},
    }

    final = {
        2: {'32M': {}, '64M': {}, '128M': {}, '256M':{}},
        4: {'32M': {}, '64M': {}, '128M': {}, '256M':{}},
        8: {'32M': {}, '64M': {}, '128M': {}, '256M':{}},
    }

    for p_count, results in final_results.items():
        for size, values in results.items():
            if p_count != 6:
                size = str(size // 1000000) + 'M'
                if size not in ('1M', '16M'):
                    final[p_count][size]['Phase 1'] = values[1]
                    final[p_count][size]['Phase 2'] = values[2]
                    final[p_count][size]['Phase 3'] = values[3]
                    final[p_count][size]['Phase 4'] = values[4]

    l = []
    keyz = []
    for key in final:
        p0 = pd.DataFrame(final[key]).transpose()
        p0 = p0.reindex(sorted(p0.columns), axis=1)
        l.append(p0)
        keyz.append(key)

    df = pd.concat(l, keys=keyz)
    return df, l

def create_pbp_perc(final_results: dict):
    final = {
        "Phase 1": {2: {}, 4: {}, 6: {}, 8: {}},
        "Phase 2": {2: {}, 4: {}, 6: {}, 8: {}},
        "Phase 3": {2: {}, 4: {}, 6: {}, 8: {}},
        "Phase 4": {2: {}, 4: {}, 6: {}, 8: {}},
    }

    for p_count, results in final_results.items():
        for size, values in results.items():
            final['Phase 0'][p_count][size] = values[0]
            final['Phase 1'][p_count][size] = values[1]
            final['Phase 2'][p_count][size] = values[2]
            final['Phase 3'][p_count][size] = values[3]
            final['Phase 4'][p_count][size] = values[4]
            final['Phase Merge'][p_count][size] = values[5]
    
    p0 = pd.DataFrame(final['Phase 0']).transpose()
    p0 = p0.reindex(sorted(p0.columns), axis=1)

    p1 = pd.DataFrame(final['Phase 1']).transpose()
    p1 = p1.reindex(sorted(p1.columns), axis=1)

    p2 = pd.DataFrame(final['Phase 2']).transpose()
    p2 = p2.reindex(sorted(p2.columns), axis=1)

    p3 = pd.DataFrame(final['Phase 3']).transpose()
    p3 = p3.reindex(sorted(p3.columns), axis=1)

    p4 = pd.DataFrame(final['Phase 4']).transpose()
    p4 = p4.reindex(sorted(p4.columns), axis=1)

    pMerge = pd.DataFrame(final['Phase Merge']).transpose()
    pMerge = pMerge.reindex(sorted(pMerge.columns), axis=1)

    df = pd.concat([p0, p1, p2, p3, p4, pMerge], keys=["Phase 0", "Phase 1", "Phase 2", "Phase 3", "Phase 4", "Phase Merge"])
    return df

def speedup_graph(df):
    import matplotlib.patches as mpatches

    plt.rcParams['figure.figsize'] = [13, 13]

    plt.figure()
    plt.grid()

    x = df['Linear']

    plt.xticks([1, 2, 3, 4, 5, 6, 7, 8], fontsize=20)
    plt.yticks(np.linspace(1, 8, 15), fontsize=20)
    plt.xlabel("processors", fontdict={'size':20})
    plt.ylabel("speedup", fontdict={'size':20})
    
    colors = {}
    colors['Linear'] = 'kx--'
    colors[1000000] = 'yo-'
    colors[16000000] = 'bo-'
    colors[32000000] = 'ro-'
    colors[64000000] = 'go-'
    colors[128000000] = 'mo-'
    colors[256000000] = 'co-'

    legendz = {}
    legendz['Linear'] = ('black', 'Unit-Linear', 'p')
    legendz[1000000] = ('yellow', '1M', 'o')
    legendz[16000000] = ('blue', '16M', 'D')
    legendz[32000000] = ('red', '32M', 'H')
    legendz[64000000] = ('green', '64M', '*')
    legendz[128000000] = ('magenta', '128M', 'X')
    legendz[256000000] = ('cyan', '256M', '>')

    handles = []
    for _, v in legendz.items():
        p = mpatches.Patch(color=v[0], label=v[1])
        handles.append(p)
    plt.legend(handles=handles, prop={'size':15})

    for c in df.columns:
        y = df[c]
        plt.plot(x, y, colors[c], label=c, marker=legendz[c][2], markersize=15)
    plt.savefig("./speedup.png")

def phase_by_phase_graph(df):
    colz = list(map(lambda x: str(x // 1000000)+"M", df.columns))
    print(colz)
    print(df.columns)
    employees=colz
    earnings={
        "2":[10,20,15,18,14,20],
        "4":[20,13,10,18,15,23],
        "8":[20,20,10,15,18, 21],
    }

    df=pd.DataFrame(earnings,index=employees)

    df.plot(kind="bar",stacked=True,figsize=(10,8))
    plt.legend(loc="lower left",bbox_to_anchor=(0.8,1.0))
    plt.savefig("./bar.png")

def plot_clustered_stacked(dfall, labels=None, title="multiple stacked bar plot",  H="/", **kwargs):
    """Given a list of dataframes, with identical columns and index, create a clustered stacked bar plot. 
        labels is a list of the names of the dataframe, used for the legend
        title is a string for the title of the plot
        H is the hatch used for identification of the different dataframe"""
    plt.rcParams['figure.figsize'] = [13, 13]

    n_df = len(dfall)
    n_col = len(dfall[0].columns) 
    n_ind = len(dfall[0].index)
    axe = plt.subplot()

    for df in dfall : # for each data frame
        axe = df.plot(kind="bar",
                      linewidth=0.2,
                      stacked=True,
                      ax=axe,
                      legend=False,
                      grid=False,
                      **kwargs)  # make bar plots

    h,l = axe.get_legend_handles_labels() # get the handles we want to modify
    for i in range(0, n_df * n_col, n_col): # len(h) = n_col * n_df
        for j, pa in enumerate(h[i:i+n_col]):
            for rect in pa.patches: # for each index
                rect.set_x(rect.get_x() + 1 / float(n_df + 1) * i / float(n_col) - 0.2)
                rect.set_width(1 / float(n_df + 1))
                rect.set_label('AA')

    axe.set_xticks((np.arange(0, 2 * n_ind, 2) + 1 / float(n_df + 1)) / 2.)
    axe.set_xticklabels(df.index, rotation = 0)
    axe.set_title(title)

    # Add invisible data to add another legend
    n=[]        
    for i in range(n_df):
        n.append(axe.bar(0, 0, color="gray", hatch=H * i))

    l1 = axe.legend(h[:n_col], l[:n_col], loc=[1.01, 0.5])
    if labels is not None:
        l2 = plt.legend(n, labels, loc=[1.01, 0.1]) 
    axe.add_artist(l1)
    return axe

if __name__ == "__main__":
    import glob

    RESULTS = dict()

    for file in glob.glob(r"./data/*.txt", recursive=True):
        res = calculate_single_iteration(file)
        key = (res.size, res.process_count)
        if key not in RESULTS:
            RESULTS[key] = []
        RESULTS[key].append(res)
    
    FINAL_RESULTS = dict()
    for i in range(2, 9, 2):
        FINAL_RESULTS[i] = dict()

    for key, results in RESULTS.items():
        p0_time = statistics.mean(map(lambda sr: sr.phase0_times, results))
        p1_time = statistics.mean(map(lambda sr: sr.phase1_times[0], results))
        p2_time = statistics.mean(map(lambda sr: sr.phase2_times[0], results))
        p3_time = statistics.mean(map(lambda sr: sr.phase3_times[0], results))
        p4_time = statistics.mean(map(lambda sr: sr.phase4_times[0], results))
        pMerge_time = statistics.mean(map(lambda sr: sr.phase_merge_times, results))
        total_time = statistics.mean(map(lambda sr: sr.total_time, results))
        rdfa = statistics.mean(map(lambda sr: sr.rdfa, results))

        size, pc = key
        FINAL_RESULTS[pc][size] = (
           p0_time,
           p1_time,
           p2_time,
           p3_time,
           p4_time,
           pMerge_time,
           total_time,
           rdfa
        )

    real_times_df = create_real_times_table(FINAL_RESULTS)
    speedup_df = create_speedup_table(real_times_df)
    rdfa_df = create_rdfa_table(FINAL_RESULTS)
    pbp_df, dfs = create_phase_by_phase_table_new(FINAL_RESULTS)

    # print(speedup_df)

    speedup_graph(speedup_df)
    # phase_by_phase_graph(pbp_df)

    # print(pbp_df.columns)

    # pbp_df = pbp_df.drop(['Phase 0', 'Phase Merge'], axis=1)

    # plot_clustered_stacked(dfs).get_figure().savefig("./aaa.png")

    for d in dfs:
        print(d)
        print()

    # w = pd.ExcelWriter("./results/data_new_1.xlsx")

    # real_times_df.to_excel(w, sheet_name="Real times")
    # speedup_df.to_excel(w, sheet_name="Speedup")
    # rdfa_df.to_excel(w, sheet_name="RDFA")
    # pbp_df.to_excel(w, sheet_name="Phase by phase")

    # w.save()
